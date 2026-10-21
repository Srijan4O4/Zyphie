#include "zyphie/archive.hpp"
#include "filesystem/io.hpp"
#include "compression/codec.hpp"
#include <algorithm>
#include <bit>
#include <chrono>
#include <map>
#include <limits>

namespace zyphie {
namespace fs = std::filesystem;
using namespace binary;
namespace {
struct Block {
    std::uint64_t offset = 0;
    std::uint32_t stored = 0, original = 0;
    std::uint8_t codec = 0;
    Digest digest{};
};
void cancelled(std::stop_token token) {
    require(!token.stop_requested(), ErrorCode::cancelled, "Operation cancelled");
}
std::size_t memory_size(std::uint64_t size) {
    require(size <= std::numeric_limits<std::size_t>::max(), ErrorCode::resource_limit, "Size exceeds address space");
    return static_cast<std::size_t>(size);
}
void read_digest(Decoder& d, Digest& result) {
    auto bytes = d.bytes(32); std::copy(bytes.begin(), bytes.end(), result.begin());
}
std::int64_t modified(const fs::path& path) {
    const auto time = fs::file_time_type::clock::to_sys(fs::last_write_time(path));
    const auto seconds = std::chrono::floor<std::chrono::seconds>(time).time_since_epoch().count();
    require(seconds >= -2147483648LL && seconds <= 4102444800LL,
        ErrorCode::unsupported_feature, "Modification time outside V1 range: " + io::utf8(path));
    return seconds;
}
void restore_time(const fs::path& path, const ArchiveEntry& entry) {
    if (entry.modified) {
        const auto time = std::chrono::sys_seconds{std::chrono::seconds{*entry.modified}};
        fs::last_write_time(path, std::chrono::time_point_cast<fs::file_time_type::duration>(fs::file_time_type::clock::from_sys(time)));
    }
}
fs::path absolute_clean(const fs::path& value) {
    auto result = fs::absolute(value).lexically_normal();
    while (result.filename().empty() && result.parent_path() != result) result = result.parent_path();
    return result;
}
fs::path destination_path(const fs::path& value) {
    require(!value.empty(), ErrorCode::invalid_argument, "Destination is required");
    const auto path = absolute_clean(value);
    io::reject_links(path);
    require(!fs::exists(path), ErrorCode::destination_exists, "Destination already exists: " + io::utf8(path));
    require(fs::is_directory(path.parent_path()), ErrorCode::io, "Destination parent must be an existing directory");
    return path;
}
bool inside(const fs::path& path, const fs::path& directory) {
    // Compare existing filesystem identities, including Windows case aliases and volume boundaries.
    for (auto ancestor = path.parent_path();;) {
        if (fs::equivalent(ancestor,directory)) return true;
        const auto parent = ancestor.parent_path();
        if (parent == ancestor || parent.empty()) return false;
        ancestor = parent;
    }
}
struct Source { fs::path native; ArchiveEntry entry; fs::file_time_type stamp; };
std::vector<Source> scan(const std::vector<fs::path>& paths, const fs::path& destination, const PackOptions& options) {
    require(!paths.empty(), ErrorCode::invalid_argument, "At least one source is required");
    std::vector<Source> result;
    std::uint64_t total = 0, metadata = 32;
    auto append = [&](const fs::path& native, const std::string& archive_path) {
        cancelled(options.cancellation); io::reject_links(native); format::validate_path(archive_path);
        require(result.size() < options.limits.entries, ErrorCode::resource_limit, "Entry count limit exceeded");
        metadata = add(metadata, add(72, archive_path.size()));
        require(metadata <= options.limits.metadata_bytes, ErrorCode::resource_limit, "Manifest size limit exceeded");
        const auto status = fs::symlink_status(native);
        Source source; source.native = native; source.entry.path = archive_path;
        source.stamp = fs::last_write_time(native); source.entry.modified = modified(native);
        if (fs::is_directory(status)) { source.entry.type = EntryType::directory; }
        else {
            require(fs::is_regular_file(status), ErrorCode::unsupported_feature, "Unsupported filesystem entry: " + io::utf8(native));
            source.entry.size = fs::file_size(native);
            require(source.entry.size <= options.limits.file_bytes, ErrorCode::resource_limit, "File size limit exceeded");
            total = add(total, source.entry.size);
            require(total <= options.limits.total_bytes, ErrorCode::resource_limit, "Total input size limit exceeded");
        }
        result.push_back(std::move(source));
    };
    for (const auto& input : paths) {
        const auto root = absolute_clean(input); io::reject_links(root);
        const auto base = io::utf8(root.filename()); format::validate_path(base);
        if (fs::is_directory(root)) {
            require(!inside(destination, root), ErrorCode::invalid_argument, "Archive destination cannot be inside a source directory");
            append(root, base);
            for (const auto& entry : fs::recursive_directory_iterator(root)) {
                append(entry.path(), base + "/" + io::utf8(entry.path().lexically_relative(root)));
            }
        } else { append(root, base); }
    }
    std::sort(result.begin(), result.end(), [](const Source& a, const Source& b) { return a.entry.path < b.entry.path; });
    for (std::size_t i = 1; i < result.size(); ++i) {
        require(result[i-1].entry.path != result[i].entry.path, ErrorCode::invalid_argument, "Duplicate input archive path: " + result[i].entry.path);
    }
    return result;
}
Bytes manifest_bytes(const std::vector<Source>& sources) {
    Encoder e; e.text("ZYPHMAN1"); e.integer(sources.size(), 8);
    for (const auto& source : sources) {
        const auto& entry = source.entry;
        e.integer(static_cast<std::uint8_t>(entry.type),1); e.integer(entry.modified ? 1 : 0,1); e.zeros(2);
        e.integer(entry.path.size(),4); e.integer(entry.size,8);
        e.integer(std::bit_cast<std::uint64_t>(entry.modified.value_or(0)),8);
        e.integer(entry.first_block,8); e.integer(entry.block_count,8); e.bytes(entry.digest); e.text(entry.path);
    }
    return e.data;
}
Bytes index_bytes(const std::vector<Block>& blocks) {
    Encoder e; e.text("ZYPHIDX1"); e.integer(blocks.size(),8);
    for (const auto& block : blocks) {
        e.integer(block.offset,8); e.integer(block.stored,4); e.integer(block.original,4);
        e.integer(block.codec,1); e.zeros(3); e.bytes(block.digest); e.zeros(4);
    }
    return e.data;
}
}

struct ArchiveReader::Impl {
    io::File file;
    std::uint64_t length = 0;
    std::vector<ArchiveEntry> entries;
    std::vector<Block> blocks;
    explicit Impl(const fs::path& path, const Limits& limits) : file(path), length(file.size()) {
        require(length >= 192, ErrorCode::corrupt_archive, "Archive is truncated");
        require(length <= limits.archive_bytes && length <= INT64_MAX, ErrorCode::resource_limit, "Archive size limit exceeded");
        const auto footer = format::read_footer(file.read(length-128,128),length);
        const auto metadata_size = add(footer.manifest_size,footer.index_size);
        require(metadata_size <= limits.metadata_bytes, ErrorCode::resource_limit, "Metadata size limit exceeded");
        auto header = file.read(0,32); const auto block_limit = format::read_header(header);
        auto manifest = file.read(footer.manifest_offset,memory_size(footer.manifest_size));
        auto index = file.read(footer.index_offset,memory_size(footer.index_size));
        Sha256 hash; hash.update(header); hash.update(manifest); hash.update(index);
        require(hash.finish() == footer.metadata_digest, ErrorCode::corrupt_archive, "Metadata checksum mismatch");

        Decoder ix(index); ix.magic("ZYPHIDX1"); const auto count = ix.integer(8);
        require(count <= limits.blocks, ErrorCode::resource_limit, "Block count limit exceeded");
        require(add(16,multiply(count,56)) == index.size(), ErrorCode::corrupt_archive, "Invalid block index size");
        blocks.reserve(memory_size(count)); std::uint64_t offset = 32;
        for (std::uint64_t i=0;i<count;++i) {
            Block block; block.offset = ix.integer(8); block.stored = static_cast<std::uint32_t>(ix.integer(4));
            block.original = static_cast<std::uint32_t>(ix.integer(4)); block.codec = static_cast<std::uint8_t>(ix.integer(1));
            ix.zeros(3); read_digest(ix,block.digest); ix.zeros(4);
            require(block.codec <= 1, ErrorCode::unsupported_feature, "Unsupported block codec");
            require(block.offset == offset && block.original > 0 && block.original <= block_limit &&
                block.stored > 0 && block.stored <= block.original && (block.codec != 0 || block.stored == block.original),
                ErrorCode::corrupt_archive, "Invalid block bounds or lengths");
            offset = add(offset,block.stored);
            require(offset <= footer.manifest_offset, ErrorCode::corrupt_archive, "Block overlaps metadata"); blocks.push_back(block);
        }
        ix.end(); require(offset == footer.manifest_offset, ErrorCode::corrupt_archive, "Unindexed payload bytes");
        Decoder m(manifest); m.magic("ZYPHMAN1"); const auto entry_count = m.integer(8);
        require(entry_count <= limits.entries, ErrorCode::resource_limit, "Entry count limit exceeded");
        require(entry_count <= (manifest.size()-16)/73, ErrorCode::corrupt_archive, "Impossible entry count");
        entries.reserve(memory_size(entry_count)); std::map<std::string,EntryType> paths;
        std::uint64_t next_block = 0, total = 0;
        for (std::uint64_t i=0;i<entry_count;++i) {
            ArchiveEntry entry; const auto type = m.integer(1), flags = m.integer(1); m.zeros(2);
            require(type == 1 || type == 2, ErrorCode::unsupported_feature, "Unsupported entry type");
            require(flags <= 1, ErrorCode::unsupported_feature, "Unsupported entry flags");
            entry.type = static_cast<EntryType>(type); const auto path_size = m.integer(4);
            require(path_size > 0 && path_size <= 4096, ErrorCode::unsafe_path, "Invalid path length");
            entry.size = m.integer(8); const auto timestamp = std::bit_cast<std::int64_t>(m.integer(8));
            if (flags) {
                require(timestamp >= -2147483648LL && timestamp <= 4102444800LL, ErrorCode::corrupt_archive, "Invalid timestamp range");
                entry.modified = timestamp;
            } else require(timestamp == 0, ErrorCode::corrupt_archive, "Absent timestamp must be zero");
            entry.first_block = m.integer(8); entry.block_count = m.integer(8); read_digest(m,entry.digest);
            const auto path_bytes = m.bytes(memory_size(path_size));
            entry.path.assign(reinterpret_cast<const char*>(path_bytes.data()),path_bytes.size()); format::validate_path(entry.path);
            require(entries.empty() || entries.back().path < entry.path, ErrorCode::corrupt_archive, "Duplicate or unsorted paths");
            if (auto slash = entry.path.rfind('/'); slash != std::string::npos) {
                auto parent = paths.find(entry.path.substr(0,slash));
                require(parent != paths.end() && parent->second == EntryType::directory, ErrorCode::corrupt_archive, "Missing directory parent");
            }
            paths.emplace(entry.path,entry.type);
            if (entry.type == EntryType::directory) {
                require(entry.size == 0 && entry.first_block == 0 && entry.block_count == 0 && entry.digest == Digest{},
                    ErrorCode::corrupt_archive, "Invalid directory fields");
            } else {
                require(entry.size <= limits.file_bytes, ErrorCode::resource_limit, "File output size limit exceeded");
                total = add(total,entry.size); require(total <= limits.total_bytes, ErrorCode::resource_limit, "Total output size limit exceeded");
                require(entry.first_block == next_block && entry.block_count <= count-next_block,
                    ErrorCode::corrupt_archive, "Invalid file block mapping");
                std::uint64_t decoded = 0;
                for (std::uint64_t j=0;j<entry.block_count;++j) {
                    const auto& b = blocks[memory_size(next_block++)]; decoded = add(decoded,b.original);
                    entry.stored_size = add(entry.stored_size,b.stored);
                }
                require(decoded == entry.size, ErrorCode::corrupt_archive, "File size differs from block mapping");
                if (!entry.size) require(entry.digest == sha256({}), ErrorCode::corrupt_archive, "Invalid empty file digest");
            }
            entries.push_back(std::move(entry));
        }
        m.end(); require(next_block == count, ErrorCode::corrupt_archive, "Unreferenced blocks");
    }
    void decode_file(const ArchiveEntry& entry, io::File* output, std::stop_token token) const {
        Sha256 hash;
        for (std::uint64_t j=0;j<entry.block_count;++j) {
            cancelled(token); const auto& block = blocks[memory_size(entry.first_block+j)];
            auto data = file.read(block.offset,block.stored);
            if (block.codec == 1) data = codec::decompress(data,block.original);
            require(sha256(data) == block.digest, ErrorCode::corrupt_archive, "Block checksum mismatch: " + entry.path);
            hash.update(data); if (output) output->write(data);
        }
        require(hash.finish() == entry.digest, ErrorCode::corrupt_archive, "File checksum mismatch: " + entry.path);
    }
};
ArchiveReader::ArchiveReader(const fs::path& archive, Limits limits) {
    try { io::reject_links(archive); impl_ = std::make_unique<Impl>(archive,limits); }
    catch (const fs::filesystem_error& e) { throw Error(ErrorCode::io,e.what()); }
}
ArchiveReader::~ArchiveReader() = default;
const std::vector<ArchiveEntry>& ArchiveReader::entries() const noexcept { return impl_->entries; }
std::uint64_t ArchiveReader::archive_size() const noexcept { return impl_->length; }
std::uint64_t ArchiveReader::block_count() const noexcept { return impl_->blocks.size(); }
std::uint64_t ArchiveReader::zstandard_blocks() const noexcept {
    return static_cast<std::uint64_t>(std::count_if(impl_->blocks.begin(),impl_->blocks.end(),[](const Block& b){return b.codec == 1;}));
}
void ArchiveReader::verify(std::stop_token cancellation) const {
    cancelled(cancellation);
    for (const auto& entry : impl_->entries) {
        cancelled(cancellation); if (entry.type == EntryType::file) impl_->decode_file(entry,nullptr,cancellation);
    }
}
void ArchiveReader::extract(const fs::path& destination, std::stop_token cancellation) const {
    try {
        cancelled(cancellation); const auto final = destination_path(destination);
        io::Temporary staging; staging.directory = true;
        for (int attempt=0;attempt<8 && !staging.active;++attempt) {
            staging.path = io::temporary_name(final.parent_path()); staging.active = fs::create_directory(staging.path);
        }
        require(staging.active, ErrorCode::io, "Cannot create private extraction directory");
#ifndef _WIN32
        fs::permissions(staging.path,fs::perms::owner_all,fs::perm_options::replace);
#endif
        for (const auto& entry : impl_->entries) {
            cancelled(cancellation); const auto path = staging.path / io::native(entry.path); io::reject_links(path);
            if (entry.type == EntryType::directory) {
                require(fs::create_directory(path), ErrorCode::destination_exists, "Destination name collision: " + entry.path);
            } else {
                io::File output(path,true); impl_->decode_file(entry,&output,cancellation); output.finish(); restore_time(path,entry);
            }
        }
        for (auto it=impl_->entries.rbegin();it!=impl_->entries.rend();++it) {
            if (it->type == EntryType::directory) restore_time(staging.path / io::native(it->path),*it);
        }
        cancelled(cancellation); io::publish(staging.path,final); staging.active = false;
    } catch (const fs::filesystem_error& e) { throw Error(ErrorCode::io,e.what()); }
}
void ArchiveWriter::pack(const std::vector<fs::path>& sources, const fs::path& destination, const PackOptions& options) {
    try {
        cancelled(options.cancellation);
        require(options.block_size > 0 && options.block_size <= format::max_block, ErrorCode::invalid_argument, "Invalid block size");
        require(options.compression_level >= 1 && options.compression_level <= 19, ErrorCode::invalid_argument, "Compression level must be 1..19");
        const auto final = destination_path(destination); auto scanned = scan(sources,final,options);
        io::Temporary staging; std::unique_ptr<io::File> output;
        for (int attempt=0;attempt<8 && !output;++attempt) {
            staging.path = io::temporary_name(final.parent_path());
            try { output = std::make_unique<io::File>(staging.path,true); staging.active = true; }
            catch (const Error& e) { if (e.code() != ErrorCode::destination_exists) throw; }
        }
        require(output != nullptr, ErrorCode::io, "Cannot create temporary archive");
        const auto header = format::header(options.block_size); output->write(header);
        std::uint64_t metadata_size = 32;
        for (const auto& source : scanned) metadata_size = add(metadata_size,add(72,source.entry.path.size()));
        std::vector<Block> blocks;
        for (auto& source : scanned) {
            cancelled(options.cancellation); auto& entry = source.entry;
            if (entry.type == EntryType::directory) continue;
            io::reject_links(source.native); io::File input(source.native);
            require(input.size() == entry.size && fs::last_write_time(source.native) == source.stamp,
                ErrorCode::source_changed, "Source changed before reading: " + entry.path);
            entry.first_block = blocks.size(); Sha256 file_hash;
            for (std::uint64_t offset=0;offset<entry.size;) {
                cancelled(options.cancellation);
                require(blocks.size() < options.limits.blocks, ErrorCode::resource_limit, "Block count limit exceeded");
                metadata_size = add(metadata_size,56);
                require(metadata_size <= options.limits.metadata_bytes, ErrorCode::resource_limit, "Metadata limit exceeded");
                const auto size = static_cast<std::uint32_t>((std::min)(entry.size-offset,std::uint64_t{options.block_size}));
                auto raw = input.read(offset,size); file_hash.update(raw);
                Block block; block.offset = output->position(); block.original = size; block.digest = sha256(raw);
                auto encoded = options.store ? Bytes{} : codec::compress(raw,options.compression_level);
                if (!options.store && encoded.size() < raw.size()) { block.codec = 1; }
                else encoded = std::move(raw);
                block.stored = static_cast<std::uint32_t>(encoded.size());
                require(add(add(output->position(),encoded.size()),add(metadata_size,128)) <= options.limits.archive_bytes,
                    ErrorCode::resource_limit, "Archive size limit exceeded");
                output->write(encoded); blocks.push_back(block); offset += size;
            }
            require(input.size() == entry.size && fs::last_write_time(source.native) == source.stamp,
                ErrorCode::source_changed, "Source changed while reading: " + entry.path);
            entry.block_count = blocks.size()-entry.first_block; entry.digest = file_hash.finish();
        }
        for (const auto& source : scanned) require(fs::last_write_time(source.native) == source.stamp,
            ErrorCode::source_changed, "Source tree changed during archiving: " + source.entry.path);
        auto manifest = manifest_bytes(scanned); auto index = index_bytes(blocks);
        format::Footer footer; footer.manifest_offset = output->position(); footer.manifest_size = manifest.size();
        footer.index_offset = add(footer.manifest_offset,footer.manifest_size); footer.index_size = index.size();
        footer.archive_size = add(add(footer.index_offset,footer.index_size),128);
        require(footer.archive_size <= options.limits.archive_bytes, ErrorCode::resource_limit, "Archive size limit exceeded");
        Sha256 hash; hash.update(header); hash.update(manifest); hash.update(index); footer.metadata_digest = hash.finish();
        output->write(manifest); output->write(index); output->write(format::footer(footer)); output->finish(); output.reset();
        { ArchiveReader check(staging.path,options.limits); check.verify(options.cancellation); }
        cancelled(options.cancellation); io::publish(staging.path,final); staging.active = false;
    } catch (const fs::filesystem_error& e) { throw Error(ErrorCode::io,e.what()); }
}
}
