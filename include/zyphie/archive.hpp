#pragma once
#include "zyphie/digest.hpp"
#include "zyphie/format.hpp"
#include <filesystem>
#include <functional>
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
enum class Phase { scan, pack, verify, extract };
struct Progress {
    Phase phase = Phase::scan;
    std::uint64_t bytes_done = 0, bytes_total = 0, entries_done = 0, entries_total = 0;
    std::string path;
};
// Called only on the operation's caller thread. Throwing aborts before publication.
// scan totals are unknown; byte completion does not mean final publication succeeded.
using ProgressCallback = std::function<void(const Progress&)>;
struct PackOptions {
    bool store = false;
    int compression_level = 3;
    std::uint32_t block_size = format::default_block;
    unsigned threads = 1; // 1..16; at most 2*threads blocks in flight.
    std::uint64_t working_memory_bytes = 512ULL * 1024 * 1024;
    Limits limits;
    std::stop_token cancellation;
    ProgressCallback progress;
};
struct ExtractOptions {
    // Exact case-sensitive archive paths. Directories include their descendants.
    // Empty selects all; selected files retain their archive paths and parents.
    std::vector<std::string> paths;
    std::stop_token cancellation;
    ProgressCallback progress;
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
    void verify(std::stop_token cancellation = {}, ProgressCallback progress = {}) const;
    // Publishes a new destination tree only. Existing destinations are never replaced.
    void extract(const std::filesystem::path& destination, std::stop_token cancellation = {}) const;
    void extract(const std::filesystem::path& destination, const ExtractOptions& options) const;
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
