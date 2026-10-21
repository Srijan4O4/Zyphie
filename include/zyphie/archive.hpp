#pragma once
#include "zyphie/digest.hpp"
#include "zyphie/format.hpp"
#include <filesystem>
#include <optional>
#include <stop_token>
#include <vector>
namespace zyphie {
struct Limits {
    std::uint64_t entries = 100000, blocks = 1000000;
    std::uint64_t metadata_bytes = 64ULL * 1024 * 1024;
    std::uint64_t file_bytes = 1ULL << 40, total_bytes = 4ULL << 40, archive_bytes = 8ULL << 40;
};
enum class EntryType : std::uint8_t { file = 1, directory = 2 };
struct ArchiveEntry {
    std::string path;
    EntryType type = EntryType::file;
    std::uint64_t size = 0, first_block = 0, block_count = 0, stored_size = 0;
    std::optional<std::int64_t> modified;
    Digest digest{};
};
struct PackOptions {
    bool store = false;
    int compression_level = 3;
    std::uint32_t block_size = format::default_block;
    Limits limits;
    std::stop_token cancellation;
};
class ArchiveReader {
public:
    explicit ArchiveReader(const std::filesystem::path& archive, Limits limits = {});
    ~ArchiveReader();
    ArchiveReader(const ArchiveReader&) = delete;
    ArchiveReader& operator=(const ArchiveReader&) = delete;
    [[nodiscard]] const std::vector<ArchiveEntry>& entries() const noexcept;
    [[nodiscard]] std::uint64_t archive_size() const noexcept;
    [[nodiscard]] std::uint64_t block_count() const noexcept;
    [[nodiscard]] std::uint64_t zstandard_blocks() const noexcept;
    void verify(std::stop_token cancellation = {}) const;
    // Milestone 2 publishes a new destination tree only. Existing destinations are never replaced.
    void extract(const std::filesystem::path& destination, std::stop_token cancellation = {}) const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
class ArchiveWriter {
public:
    static void pack(const std::vector<std::filesystem::path>& sources,
        const std::filesystem::path& destination, const PackOptions& options = {});
};
}
