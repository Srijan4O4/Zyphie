#pragma once
#include "zyphie/binary.hpp"
#include <filesystem>
#include <memory>
#include <functional>
namespace zyphie::io {
// Internal deterministic fault seam. No environment variables or archive-controlled hooks.
enum class Event { read, write, flush, publish, mkdir };
void checkpoint(Event event);
#ifdef ZYPH_TEST_HOOKS
extern thread_local std::function<void(Event)> test_hook;
#endif
bool create_directory(const std::filesystem::path& path);
void set_time(const std::filesystem::path& path, std::int64_t seconds);
std::string utf8(const std::filesystem::path& path);
std::filesystem::path native(std::string_view path);
void reject_links(const std::filesystem::path& path);
void publish(const std::filesystem::path& temporary, const std::filesystem::path& destination);
std::filesystem::path temporary_name(const std::filesystem::path& parent);
class File {
public:
    explicit File(const std::filesystem::path& path, bool create = false);
    ~File();
    File(const File&) = delete;
    File& operator=(const File&) = delete;
    std::uint64_t size() const;
    binary::Bytes read(std::uint64_t offset, std::size_t length) const;
    void write(std::span<const std::uint8_t> bytes);
    void finish();
    std::uint64_t position() const noexcept { return position_; }
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    std::uint64_t position_ = 0;
};
// Owns only a newly-created staging path. Source and final destination are never cleanup targets.
class Temporary {
public:
    std::filesystem::path path;
    bool directory = false, active = false;
    ~Temporary();
};
}
