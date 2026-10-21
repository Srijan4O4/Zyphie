#pragma once
#include <array>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
namespace zyphie {
using Digest = std::array<std::uint8_t, 32>;
class Sha256 {
public:
    Sha256();
    ~Sha256();
    Sha256(const Sha256&) = delete;
    Sha256& operator=(const Sha256&) = delete;
    void update(std::span<const std::uint8_t> bytes);
    Digest finish();
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
Digest sha256(std::span<const std::uint8_t> bytes);
std::string hex(const Digest& digest);
}
