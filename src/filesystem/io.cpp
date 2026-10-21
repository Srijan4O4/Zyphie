#include "filesystem/io.hpp"
#include <algorithm>
#include <cerrno>
#include <limits>
#include <random>
#include <system_error>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#ifdef __linux__
#include <sys/syscall.h>
#endif
#endif
namespace zyphie::io {
namespace fs = std::filesystem;
std::string utf8(const fs::path& path) {
    auto value = path.generic_u8string(); return {reinterpret_cast<const char*>(value.data()), value.size()};
}
fs::path native(std::string_view path) {
    return fs::path(std::u8string(reinterpret_cast<const char8_t*>(path.data()), path.size()));
}
void reject_links(const fs::path& path) {
    fs::path current;
    for (const auto& part : fs::absolute(path).lexically_normal()) {
        current /= part;
        std::error_code ec; const auto status = fs::symlink_status(current, ec);
        if (ec == std::errc::no_such_file_or_directory || status.type() == fs::file_type::not_found) break;
        require(!ec, ErrorCode::io, "Cannot inspect path: " + utf8(current));
        require(!fs::is_symlink(status), ErrorCode::unsafe_path, "Symbolic link is unsupported: " + utf8(current));
#ifdef _WIN32
        const auto attributes = GetFileAttributesW(current.c_str());
        require(attributes != INVALID_FILE_ATTRIBUTES, ErrorCode::io, "Cannot inspect Windows path");
        require((attributes & FILE_ATTRIBUTE_REPARSE_POINT) == 0, ErrorCode::unsafe_path, "Reparse point is unsupported: " + utf8(current));
#endif
    }
}
struct File::Impl {
#ifdef _WIN32
    HANDLE handle = INVALID_HANDLE_VALUE;
    ~Impl() { if (handle != INVALID_HANDLE_VALUE) CloseHandle(handle); }
#else
    int handle = -1;
    ~Impl() { if (handle >= 0) ::close(handle); }
#endif
};
File::File(const fs::path& path, bool create) : impl_(std::make_unique<Impl>()) {
#ifdef _WIN32
    impl_->handle = CreateFileW(path.c_str(), create ? GENERIC_WRITE : GENERIC_READ,
        create ? 0 : FILE_SHARE_READ, nullptr, create ? CREATE_NEW : OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
    if (impl_->handle == INVALID_HANDLE_VALUE) {
        const auto error = GetLastError();
        throw Error(error == ERROR_FILE_EXISTS || error == ERROR_ALREADY_EXISTS ? ErrorCode::destination_exists : ErrorCode::io,
            "Cannot open file: " + utf8(path) + " (Windows error " + std::to_string(error) + ")");
    }
    BY_HANDLE_FILE_INFORMATION info{};
    require(GetFileInformationByHandle(impl_->handle, &info) != 0 &&
        !(info.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)),
        ErrorCode::unsafe_path, "Only regular non-reparse files are supported");
#else
    impl_->handle = ::open(path.c_str(), (create ? O_WRONLY | O_CREAT | O_EXCL : O_RDONLY | O_NONBLOCK) | O_NOFOLLOW | O_CLOEXEC, 0600);
    if (impl_->handle < 0) throw Error(errno == EEXIST ? ErrorCode::destination_exists : ErrorCode::io, "Cannot open file: " + utf8(path));
    struct stat info{};
    require(::fstat(impl_->handle, &info) == 0 && S_ISREG(info.st_mode), ErrorCode::unsafe_path, "Only regular files are supported");
#endif
}
File::~File() = default;
std::uint64_t File::size() const {
#ifdef _WIN32
    LARGE_INTEGER length{};
    require(GetFileSizeEx(impl_->handle, &length) && length.QuadPart >= 0, ErrorCode::io, "Cannot read file size");
    return static_cast<std::uint64_t>(length.QuadPart);
#else
    struct stat info{}; require(::fstat(impl_->handle, &info) == 0 && info.st_size >= 0, ErrorCode::io, "Cannot read file size");
    return static_cast<std::uint64_t>(info.st_size);
#endif
}
binary::Bytes File::read(std::uint64_t offset, std::size_t length) const {
    require(binary::add(offset, length) <= size() && offset <= INT64_MAX, ErrorCode::corrupt_archive, "Read outside file bounds");
    binary::Bytes result(length); std::size_t done = 0;
#ifdef _WIN32
    LARGE_INTEGER position{}; position.QuadPart = static_cast<LONGLONG>(offset);
    require(SetFilePointerEx(impl_->handle, position, nullptr, FILE_BEGIN), ErrorCode::io, "Seek failed");
#endif
    while (done < length) {
        const auto count = (std::min)(length - done, std::size_t{1048576});
#ifdef _WIN32
        DWORD got = 0;
        require(ReadFile(impl_->handle, result.data() + done, static_cast<DWORD>(count), &got, nullptr) && got != 0,
            ErrorCode::io, "File read failed or source changed");
#else
        const auto got = ::pread(impl_->handle, result.data() + done, count, static_cast<off_t>(offset + done));
        if (got < 0 && errno == EINTR) continue;
        require(got > 0, ErrorCode::io, "File read failed or source changed");
#endif
        done += static_cast<std::size_t>(got);
    }
    return result;
}
void File::write(std::span<const std::uint8_t> bytes) {
    while (!bytes.empty()) {
        const auto count = (std::min)(bytes.size(), std::size_t{1048576});
#ifdef _WIN32
        DWORD wrote = 0;
        require(WriteFile(impl_->handle, bytes.data(), static_cast<DWORD>(count), &wrote, nullptr) && wrote != 0,
            ErrorCode::io, "File write failed");
#else
        const auto wrote = ::write(impl_->handle, bytes.data(), count);
        if (wrote < 0 && errno == EINTR) continue;
        require(wrote > 0, ErrorCode::io, "File write failed");
#endif
        const auto amount = static_cast<std::size_t>(wrote);
        position_ = binary::add(position_, amount); bytes = bytes.subspan(amount);
    }
}
void File::finish() {
#ifdef _WIN32
    require(FlushFileBuffers(impl_->handle), ErrorCode::io, "Flushing file failed");
    const auto handle = impl_->handle; impl_->handle = INVALID_HANDLE_VALUE;
    require(CloseHandle(handle), ErrorCode::io, "Closing file failed");
#else
    require(::fsync(impl_->handle) == 0, ErrorCode::io, "Flushing file failed");
    const auto handle = impl_->handle; impl_->handle = -1;
    require(::close(handle) == 0, ErrorCode::io, "Closing file failed");
#endif
}
fs::path temporary_name(const fs::path& parent) {
    std::random_device random; constexpr char digits[] = "0123456789abcdef";
    std::string name = ".zyph-part-";
    for (int i = 0; i < 32; ++i) name += digits[random() & 15];
    return parent / name;
}
Temporary::~Temporary() {
    if (active) {
        std::error_code ignored;
        if (directory) fs::remove_all(path, ignored); else fs::remove(path, ignored);
    }
}
void publish(const fs::path& temporary, const fs::path& destination) {
    reject_links(destination.parent_path());
#ifdef _WIN32
    if (!MoveFileExW(temporary.c_str(), destination.c_str(), 0)) {
        const auto error = GetLastError();
        throw Error(error == ERROR_ALREADY_EXISTS || error == ERROR_FILE_EXISTS ? ErrorCode::destination_exists : ErrorCode::io,
            "Cannot publish output (Windows error " + std::to_string(error) + ")");
    }
#elif defined(__linux__)
    if (::syscall(SYS_renameat2, AT_FDCWD, temporary.c_str(), AT_FDCWD, destination.c_str(), 1) != 0) {
        const auto error = errno;
        throw Error(error == EEXIST || error == ENOTEMPTY ? ErrorCode::destination_exists : ErrorCode::io,
            "Cannot publish output: " + std::error_code(error,std::generic_category()).message());
    }
#else
    (void)temporary; (void)destination;
    throw Error(ErrorCode::unsupported_feature, "Atomic no-replace publication is not implemented on this platform");
#endif
}
}
