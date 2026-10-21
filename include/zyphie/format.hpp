#pragma once
#include "zyphie/binary.hpp"
#include "zyphie/digest.hpp"
namespace zyphie::format {
inline constexpr std::uint32_t max_block = 16 * 1024 * 1024;
inline constexpr std::uint32_t default_block = 4 * 1024 * 1024;
struct Footer {
    std::uint64_t manifest_offset{}, manifest_size{}, index_offset{}, index_size{}, archive_size{};
    Digest metadata_digest{};
};
binary::Bytes header(std::uint32_t block_limit);
std::uint32_t read_header(std::span<const std::uint8_t> bytes);
binary::Bytes footer(const Footer& value);
Footer read_footer(std::span<const std::uint8_t> bytes, std::uint64_t archive_size);
void validate_path(std::string_view path);
}
