#include "filesystem/io.hpp"
#include <algorithm>
#include <cerrno>
#include <limits>
#include <random>
#include <system_error>
#include <thread>
#include <chrono>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <sddl.h>
#include <winternl.h>
#ifdef _MSC_VER
// Exported by ntdll; some Windows SDK winternl headers omit this declaration.
extern "C" NTSYSAPI NTSTATUS NTAPI NtSetInformationFile(HANDLE,PIO_STATUS_BLOCK,PVOID,ULONG,FILE_INFORMATION_CLASS);
#endif
#else
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>
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
#ifdef _WIN32
// Open a single component relative to an already validated directory handle.
// Native flags: FILE_OPEN_REPARSE_POINT, FILE_SYNCHRONOUS_IO_NONALERT.
HANDLE relative_open(HANDLE parent,const fs::path& name,ACCESS_MASK access,ULONG share,
    ULONG disposition,ULONG kind,PSECURITY_DESCRIPTOR security=nullptr,bool allow_reparse=false) {
    const auto text=name.native();
    require(!text.empty() && text.size() <= 32767 && name == name.filename(),ErrorCode::unsafe_path,"Invalid native component");
    UNICODE_STRING unicode{};unicode.Buffer=const_cast<PWSTR>(text.c_str());
    unicode.Length=static_cast<USHORT>(text.size()*sizeof(wchar_t));unicode.MaximumLength=unicode.Length;
    OBJECT_ATTRIBUTES attributes{};attributes.Length=sizeof(attributes);attributes.RootDirectory=parent;
    attributes.ObjectName=&unicode;attributes.Attributes=0x40;attributes.SecurityDescriptor=security;
    IO_STATUS_BLOCK status{};HANDLE handle=INVALID_HANDLE_VALUE;
    const auto result=NtCreateFile(&handle,access|SYNCHRONIZE|FILE_READ_ATTRIBUTES,&attributes,&status,nullptr,
        FILE_ATTRIBUTE_NORMAL,share,disposition,0x00200000|0x20|kind,nullptr,0);
    if(result<0){SetLastError(RtlNtStatusToDosError(result));return INVALID_HANDLE_VALUE;}
    BY_HANDLE_FILE_INFORMATION info{};
    if(!GetFileInformationByHandle(handle,&info) || (!allow_reparse && (info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT))) {
        CloseHandle(handle);SetLastError(ERROR_CANT_ACCESS_FILE);return INVALID_HANDLE_VALUE;
    }
    return handle;
}
#endif

// Pins Windows ancestors against rename; relative opens prevent reparse redirection. On POSIX each
// component is opened relative to the preceding descriptor without following links.
struct Parent {
    fs::path absolute;
#ifdef _WIN32
    std::vector<HANDLE> handles;
#else
    int fd = -1;
#endif
    explicit Parent(const fs::path& path) : absolute(fs::absolute(path).lexically_normal()) {
        try {
#ifdef _WIN32
            const auto ancestors=absolute.parent_path().relative_path();
            handles.reserve(static_cast<std::size_t>(std::distance(ancestors.begin(),ancestors.end()))+1);
            fs::path current = absolute.root_path();
            auto lock = [&](const fs::path& component) {
                auto h = handles.empty() ? CreateFileW(component.c_str(), FILE_LIST_DIRECTORY | FILE_READ_ATTRIBUTES,
                    FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
                    FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr) : relative_open(handles.back(),component.filename(),FILE_LIST_DIRECTORY | FILE_READ_ATTRIBUTES,FILE_SHARE_READ | FILE_SHARE_WRITE,1,1);
                require(h != INVALID_HANDLE_VALUE, ErrorCode::unsafe_path, "Cannot lock directory ancestor");
                handles.push_back(h);
                BY_HANDLE_FILE_INFORMATION info{};
                require(GetFileInformationByHandle(h,&info) &&
                    (info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
                    !(info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT),
                    ErrorCode::unsafe_path, "Unsafe directory ancestor");
            };
            lock(current);
            for (const auto& part : absolute.parent_path().relative_path()) { current /= part; lock(current); }
#else
            fd = ::open("/", O_RDONLY | O_DIRECTORY | O_CLOEXEC);
            require(fd >= 0, ErrorCode::io, "Cannot open filesystem root");
            for (const auto& part : absolute.parent_path().relative_path()) {
                const int next = ::openat(fd,part.c_str(),O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
                require(next >= 0, ErrorCode::unsafe_path, "Unsafe or inaccessible directory ancestor");
                ::close(fd); fd = next;
            }
#endif
        } catch (...) { close(); throw; }
    }
    Parent(const Parent&) = delete;
    void close() noexcept {
#ifdef _WIN32
        for (auto h : handles) CloseHandle(h);
        handles.clear();
#else
        if (fd >= 0) ::close(fd);
        fd = -1;
#endif
    }
    ~Parent() { close(); }
};
#ifdef ZYPH_TEST_HOOKS
thread_local std::function<void(Event)> test_hook;
#endif
void checkpoint(Event event) {
#ifdef ZYPH_TEST_HOOKS
    if (test_hook) test_hook(event);
#else
    (void)event;
#endif
}
#ifdef _WIN32
struct PrivateSecurity {
    PSECURITY_DESCRIPTOR descriptor = nullptr;
    SECURITY_ATTRIBUTES attributes{sizeof(SECURITY_ATTRIBUTES),nullptr,FALSE};
    PrivateSecurity() {
        // Protected DACL: object owner and SYSTEM only, inherited by all children.
        require(ConvertStringSecurityDescriptorToSecurityDescriptorW(
            L"D:P(A;OICI;FA;;;OW)(A;OICI;FA;;;SY)",SDDL_REVISION_1,&descriptor,nullptr),
            ErrorCode::io,"Cannot construct private staging ACL");
        attributes.lpSecurityDescriptor = descriptor;
    }
    ~PrivateSecurity() { if (descriptor) LocalFree(descriptor); }
};
#endif
bool create_directory(const fs::path& path) {
    Parent parent(path); checkpoint(Event::mkdir);
#ifdef _WIN32
    PrivateSecurity security;
    auto h=relative_open(parent.handles.back(),parent.absolute.filename(),FILE_LIST_DIRECTORY | FILE_READ_ATTRIBUTES,FILE_SHARE_READ | FILE_SHARE_WRITE,2,1,security.descriptor);
    if(h!=INVALID_HANDLE_VALUE){CloseHandle(h);return true;}
    if (GetLastError() == ERROR_ALREADY_EXISTS || GetLastError() == ERROR_FILE_EXISTS) return false;
#else
    if (::mkdirat(parent.fd,parent.absolute.filename().c_str(),0700) == 0) return true;
    if (errno == EEXIST) return false;
#endif
    throw Error(ErrorCode::io,"Cannot create private directory");
}
void set_time(const fs::path& path, std::int64_t seconds) {
    Parent parent(path);
#ifdef _WIN32
    auto h=relative_open(parent.handles.back(),parent.absolute.filename(),FILE_WRITE_ATTRIBUTES,FILE_SHARE_READ,1,0);
    require(h != INVALID_HANDLE_VALUE,ErrorCode::io,"Cannot open timestamp target");
    BY_HANDLE_FILE_INFORMATION info{};
    const bool safe = GetFileInformationByHandle(h,&info) && !(info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT);
    const auto ticks = static_cast<std::uint64_t>(seconds + 11644473600LL) * 10000000ULL;
    FILETIME time{static_cast<DWORD>(ticks),static_cast<DWORD>(ticks >> 32)};
    const bool ok = safe && SetFileTime(h,nullptr,nullptr,&time);
    CloseHandle(h); require(ok,ErrorCode::io,"Cannot restore timestamp safely");
#else
    const int fd = ::openat(parent.fd,parent.absolute.filename().c_str(),O_RDONLY | O_NONBLOCK | O_NOFOLLOW | O_CLOEXEC);
    require(fd >= 0,ErrorCode::io,"Cannot open timestamp target");
    const timespec times[2]{{0,UTIME_OMIT},{static_cast<time_t>(seconds),0}};
    const bool ok = ::futimens(fd,times) == 0; ::close(fd);
    require(ok,ErrorCode::io,"Cannot restore timestamp safely");
#endif
}

struct File::Impl {
    std::unique_ptr<Parent> parent;
#ifdef _WIN32
    HANDLE handle = INVALID_HANDLE_VALUE;
    ~Impl() { if (handle != INVALID_HANDLE_VALUE) CloseHandle(handle); }
#else
    int handle = -1;
    ~Impl() { if (handle >= 0) ::close(handle); }
#endif
};
File::File(const fs::path& path, bool create) : impl_(std::make_unique<Impl>()) {
    impl_->parent = std::make_unique<Parent>(path);
#ifdef _WIN32
    PrivateSecurity security;
    impl_->handle = relative_open(impl_->parent->handles.back(),impl_->parent->absolute.filename(),
        create ? GENERIC_WRITE : GENERIC_READ,create ? 0 : FILE_SHARE_READ,create ? 2 : 1,0x40,create ? security.descriptor : nullptr);
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
    impl_->handle = ::openat(impl_->parent->fd, impl_->parent->absolute.filename().c_str(), (create ? O_WRONLY | O_CREAT | O_EXCL : O_RDONLY | O_NONBLOCK) | O_NOFOLLOW | O_CLOEXEC, 0600);
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
    checkpoint(Event::read);
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
    checkpoint(Event::write);
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
    checkpoint(Event::flush);
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
#ifndef _WIN32
namespace {
void remove_at(int parent, const char* name) noexcept {
    const int fd = ::openat(parent,name,O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    if (fd < 0) { ::unlinkat(parent,name,0); return; }
    DIR* directory = ::fdopendir(fd);
    if (!directory) { ::close(fd); return; }
    while (const auto* entry = ::readdir(directory)) {
        const std::string_view child(entry->d_name);
        if (child != "." && child != "..") remove_at(fd,entry->d_name);
    }
    ::closedir(directory);
    ::unlinkat(parent,name,AT_REMOVEDIR);
}
}
#endif
#ifdef _WIN32
namespace {
bool remove_relative(HANDLE parent,const fs::path& name) {
    // Reparse objects are opened themselves, then unlinked without enumeration.
    HANDLE h=relative_open(parent,name,DELETE|FILE_LIST_DIRECTORY,
        FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,1,0,nullptr,true);
    if(h==INVALID_HANDLE_VALUE)return false;
    struct Close { HANDLE h; ~Close(){CloseHandle(h);} } close{h};
    BY_HANDLE_FILE_INFORMATION info{};
    if(!GetFileInformationByHandle(h,&info))return false;
    if((info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && !(info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)) {
        std::vector<std::uint64_t> storage(8192);
        for(;;) {
            if(!GetFileInformationByHandleEx(h,FileIdBothDirectoryRestartInfo,storage.data(),static_cast<DWORD>(storage.size()*8))) {
                if(GetLastError()==ERROR_NO_MORE_FILES)break;
                return false;
            }
            auto* item=reinterpret_cast<FILE_ID_BOTH_DIR_INFO*>(storage.data());
            bool removed=false;
            for(;;) {
                const std::wstring child(item->FileName,item->FileNameLength/sizeof(wchar_t));
                if(child!=L"." && child!=L"..") {
                    if(!remove_relative(h,fs::path(child)))return false;
                    removed=true;break;
                }
                if(!item->NextEntryOffset)break;
                item=reinterpret_cast<FILE_ID_BOTH_DIR_INFO*>(reinterpret_cast<std::uint8_t*>(item)+item->NextEntryOffset);
            }
            if(!removed)break;
        }
    }
    FILE_DISPOSITION_INFO disposition{TRUE};
    return SetFileInformationByHandle(h,FileDispositionInfo,&disposition,sizeof(disposition))!=0;
}
}
#endif

Temporary::~Temporary() {
    if (!active) return;
    try {
        Parent parent(path);
#ifdef _WIN32
        (void)remove_relative(parent.handles.back(),parent.absolute.filename());
#else
        remove_at(parent.fd,parent.absolute.filename().c_str());
#endif
    } catch (...) { /* Isolate failed cleanup; never turn it into deletion elsewhere. */ }
}

void publish(const fs::path& temporary, const fs::path& destination, std::stop_token cancellation) {
    Parent source(temporary), target(destination);
    checkpoint(Event::publish);
    require(!cancellation.stop_requested(),ErrorCode::cancelled,"Operation cancelled");
#ifdef _WIN32
    const auto name=target.absolute.filename().native();
    const auto length=offsetof(FILE_RENAME_INFO,FileName)+name.size()*sizeof(wchar_t);
    std::vector<std::uint64_t> storage((length+7)/8,0);
    auto* rename=reinterpret_cast<FILE_RENAME_INFO*>(storage.data());
    rename->ReplaceIfExists=FALSE;rename->RootDirectory=target.handles.back();
    rename->FileNameLength=static_cast<DWORD>(name.size()*sizeof(wchar_t));
    std::copy(name.begin(),name.end(),rename->FileName);
    auto h=relative_open(source.handles.back(),source.absolute.filename(),DELETE,0,1,0);
    require(h!=INVALID_HANDLE_VALUE,ErrorCode::io,"Cannot open publication source");
    struct Close { HANDLE handle; ~Close(){CloseHandle(handle);} } close{h};
    for(unsigned attempt=0;;++attempt) {
        require(!cancellation.stop_requested(),ErrorCode::cancelled,"Operation cancelled");
        IO_STATUS_BLOCK status{};
        const auto result=NtSetInformationFile(h,&status,rename,static_cast<ULONG>(length),static_cast<FILE_INFORMATION_CLASS>(10));
        if(result>=0)break;
        const auto error=RtlNtStatusToDosError(result);
        // Child handles held briefly by indexers/antivirus can deny a directory rename.
        // Every attempt retains the same pinned parents and ReplaceIfExists=FALSE.
        if(attempt<8 && (error==ERROR_ACCESS_DENIED || error==ERROR_SHARING_VIOLATION)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(25));continue;
        }
        throw Error(error==ERROR_ALREADY_EXISTS || error==ERROR_FILE_EXISTS ? ErrorCode::destination_exists : ErrorCode::io,
            "Cannot publish output (Windows error "+std::to_string(error)+")");
    }
#elif defined(__linux__)
    if (::syscall(SYS_renameat2, source.fd, source.absolute.filename().c_str(), target.fd, target.absolute.filename().c_str(), 1) != 0) {
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
