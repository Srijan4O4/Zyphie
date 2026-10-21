#pragma once
#include "zyphie/binary.hpp"
namespace zyphie::codec {
binary::Bytes compress(std::span<const std::uint8_t> bytes, int level);
binary::Bytes decompress(std::span<const std::uint8_t> bytes, std::uint32_t original_size);
}
