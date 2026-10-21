#pragma once
#include "zyphie/error.hpp"
#include <cstdint>
#include <limits>
#include <span>
#include <string_view>
#include <vector>
namespace zyphie::binary {
using Bytes = std::vector<std::uint8_t>;
inline std::uint64_t add(std::uint64_t a, std::uint64_t b) {
    require(b <= std::numeric_limits<std::uint64_t>::max() - a,
        ErrorCode::corrupt_archive, "Integer addition overflow");
    return a + b;
}
inline std::uint64_t multiply(std::uint64_t a, std::uint64_t b) {
    require(a == 0 || b <= std::numeric_limits<std::uint64_t>::max() / a,
        ErrorCode::corrupt_archive, "Integer multiplication overflow");
    return a * b;
}
class Encoder {
public:
    Bytes data;
    void integer(std::uint64_t value, unsigned width) {
        require(width >= 1 && width <= 8, ErrorCode::invalid_argument, "Invalid integer width");
        require(width == 8 || value < (std::uint64_t{1} << (width * 8)),
            ErrorCode::invalid_argument, "Integer does not fit field");
        for (unsigned i = 0; i < width; ++i) {
            data.push_back(static_cast<std::uint8_t>(value & 255)); value >>= 8;
        }
    }
    void bytes(std::span<const std::uint8_t> value) { data.insert(data.end(), value.begin(), value.end()); }
    void text(std::string_view value) {
        bytes({reinterpret_cast<const std::uint8_t*>(value.data()), value.size()});
    }
    void zeros(std::size_t count) { data.insert(data.end(), count, 0); }
};
class Decoder {
public:
    explicit Decoder(std::span<const std::uint8_t> data) : data_(data) {}
    std::span<const std::uint8_t> bytes(std::size_t count) {
        require(count <= data_.size() - offset_, ErrorCode::corrupt_archive, "Truncated binary field");
        auto result = data_.subspan(offset_, count); offset_ += count; return result;
    }
    std::uint64_t integer(unsigned width) {
        require(width >= 1 && width <= 8, ErrorCode::invalid_argument, "Invalid integer width");
        auto value = bytes(width); std::uint64_t result = 0;
        for (unsigned i = 0; i < width; ++i) { result |= std::uint64_t{value[i]} << (8 * i); }
        return result;
    }
    void magic(std::string_view expected) {
        auto value = bytes(expected.size());
        require(std::string_view(reinterpret_cast<const char*>(value.data()), value.size()) == expected,
            ErrorCode::corrupt_archive, "Invalid magic signature");
    }
    void zeros(std::size_t count) {
        for (auto value : bytes(count)) { require(value == 0, ErrorCode::unsupported_feature, "Nonzero reserved field"); }
    }
    void end() const { require(offset_ == data_.size(), ErrorCode::corrupt_archive, "Unexpected trailing bytes"); }
private:
    std::span<const std::uint8_t> data_;
    std::size_t offset_ = 0;
};
}
