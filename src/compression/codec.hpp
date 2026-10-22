#pragma once
#include "zyphie/binary.hpp"
namespace zyphie::codec {
std::uint64_t workspace_bound(std::uint32_t block_size, int level);
binary::Bytes compress(std::span<const std::uint8_t> bytes, int level);
binary::Bytes decompress(std::span<const std::uint8_t> bytes, std::uint32_t original_size);
}
