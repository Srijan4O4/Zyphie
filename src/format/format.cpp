#include "zyphie/format.hpp"
#include <algorithm>
namespace zyphie::format {
using namespace binary;
Bytes header(std::uint32_t block_limit) {
    require(block_limit > 0 && block_limit <= max_block, ErrorCode::invalid_argument, "Invalid block limit");
    Encoder e; e.text("ZYPH\r\n\x1a\n"); e.integer(1, 2); e.integer(0, 2);
    e.integer(32, 4); e.zeros(4); e.integer(1, 2); e.zeros(2); e.integer(block_limit, 4); e.zeros(4);
    return e.data;
}
std::uint32_t read_header(std::span<const std::uint8_t> bytes) {
    Decoder d(bytes); d.magic("ZYPH\r\n\x1a\n");
    require(d.integer(2) == 1 && d.integer(2) == 0, ErrorCode::unsupported_feature, "Unsupported format version");
    require(d.integer(4) == 32, ErrorCode::unsupported_feature, "Unsupported header size");
    d.zeros(4); require(d.integer(2) == 1, ErrorCode::unsupported_feature, "Unsupported digest"); d.zeros(2);
    auto limit = static_cast<std::uint32_t>(d.integer(4)); d.zeros(4); d.end();
    require(limit > 0 && limit <= max_block, ErrorCode::corrupt_archive, "Invalid block size limit"); return limit;
}
Bytes footer(const Footer& value) {
    Encoder e; e.text("ZYPHEND1"); e.integer(value.manifest_offset, 8); e.integer(value.manifest_size, 8);
    e.integer(value.index_offset, 8); e.integer(value.index_size, 8); e.integer(value.archive_size, 8);
    e.zeros(16); e.bytes(value.metadata_digest); e.bytes(sha256(e.data)); return e.data;
}
Footer read_footer(std::span<const std::uint8_t> bytes, std::uint64_t archive_size) {
    require(bytes.size() == 128 && archive_size >= 192, ErrorCode::corrupt_archive, "Invalid archive/footer size");
    const auto digest = sha256(bytes.first(96));
    require(std::equal(digest.begin(), digest.end(), bytes.begin() + 96), ErrorCode::corrupt_archive, "Footer checksum mismatch");
    Decoder d(bytes); d.magic("ZYPHEND1"); Footer f;
    f.manifest_offset = d.integer(8); f.manifest_size = d.integer(8);
    f.index_offset = d.integer(8); f.index_size = d.integer(8); f.archive_size = d.integer(8);
    d.zeros(16); auto metadata = d.bytes(32); std::copy(metadata.begin(), metadata.end(), f.metadata_digest.begin());
    (void)d.bytes(32); d.end();
    require(f.archive_size == archive_size && f.manifest_offset >= 32 && f.manifest_size >= 16 && f.index_size >= 16,
        ErrorCode::corrupt_archive, "Invalid section bounds");
    require(add(f.manifest_offset, f.manifest_size) == f.index_offset &&
        add(add(f.index_offset, f.index_size), 128) == archive_size,
        ErrorCode::corrupt_archive, "Inconsistent section offsets"); return f;
}
void validate_path(std::string_view path) {
    auto bad = [](bool ok) { require(ok, ErrorCode::unsafe_path, "Unsafe or invalid archive path"); };
    bad(!path.empty() && path.size() <= 4096);
    // Decode scalar values to reject overlong UTF-8, surrogates and invalid continuation bytes.
    for (std::size_t i = 0; i < path.size();) {
        const auto first = static_cast<unsigned char>(path[i++]);
        if (first < 128) { bad(first >= 32 && first != 127); continue; }
        unsigned count = 0; std::uint32_t value = 0, minimum = 0;
        if ((first & 0xe0) == 0xc0) { count = 1; value = first & 31; minimum = 0x80; }
        else if ((first & 0xf0) == 0xe0) { count = 2; value = first & 15; minimum = 0x800; }
        else if ((first & 0xf8) == 0xf0) { count = 3; value = first & 7; minimum = 0x10000; }
        else { bad(false); }
        bad(count <= path.size() - i);
        for (unsigned j = 0; j < count; ++j) {
            auto byte = static_cast<unsigned char>(path[i++]); bad((byte & 0xc0) == 0x80); value = (value << 6) | (byte & 63);
        }
        bad(value >= minimum && value <= 0x10ffff && !(value >= 0xd800 && value <= 0xdfff));
    }
    bad(path.find_first_of("<>:\"\\|?*") == std::string_view::npos);
    for (std::size_t start = 0; start <= path.size();) {
        const auto slash = path.find('/', start); const auto end = slash == std::string_view::npos ? path.size() : slash;
        auto part = path.substr(start, end - start);
        bad(!part.empty() && part.size() <= 255 && part != "." && part != "..");
        bad(part.back() != '.' && part.back() != ' ');
        std::string base(part.substr(0, part.find('.')));
        for (auto& c : base) { if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 'a' + 'A'); }
        bad(base != "CON" && base != "PRN" && base != "AUX" && base != "NUL" && base != "CONIN$" && base != "CONOUT$");
        if (base.starts_with("COM") || base.starts_with("LPT")) {
            auto suffix = std::string_view(base).substr(3);
            bad(!(suffix.size() == 1 && suffix[0] >= '1' && suffix[0] <= '9') &&
                suffix != "\xc2\xb9" && suffix != "\xc2\xb2" && suffix != "\xc2\xb3");
        }
        if (slash == std::string_view::npos) break;
        start = slash + 1;
    }
}
}
