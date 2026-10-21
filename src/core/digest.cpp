#include "zyphie/digest.hpp"
#include "zyphie/error.hpp"
#include <algorithm>
#include <limits>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <bcrypt.h>
#else
#include <openssl/evp.h>
#endif
namespace zyphie {
struct Sha256::Impl {
    bool finished = false;
#ifdef _WIN32
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    ~Impl() { if (hash) BCryptDestroyHash(hash); if (algorithm) BCryptCloseAlgorithmProvider(algorithm, 0); }
#else
    EVP_MD_CTX* context = EVP_MD_CTX_new();
    ~Impl() { EVP_MD_CTX_free(context); }
#endif
};
Sha256::Sha256() : impl_(std::make_unique<Impl>()) {
#ifdef _WIN32
    require(BCryptOpenAlgorithmProvider(&impl_->algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) >= 0,
        ErrorCode::io, "Cannot open SHA-256 provider");
    require(BCryptCreateHash(impl_->algorithm, &impl_->hash, nullptr, 0, nullptr, 0, 0) >= 0,
        ErrorCode::io, "Cannot create SHA-256 context");
#else
    require(impl_->context && EVP_DigestInit_ex(impl_->context, EVP_sha256(), nullptr) == 1,
        ErrorCode::io, "Cannot create SHA-256 context");
#endif
}
Sha256::~Sha256() = default;
void Sha256::update(std::span<const std::uint8_t> bytes) {
    require(!impl_->finished, ErrorCode::invalid_argument, "Digest already finalized");
#ifdef _WIN32
    while (!bytes.empty()) {
        const auto size = static_cast<ULONG>((std::min)(bytes.size(), std::size_t{1048576}));
        require(BCryptHashData(impl_->hash, const_cast<PUCHAR>(bytes.data()), size, 0) >= 0,
            ErrorCode::io, "SHA-256 update failed");
        bytes = bytes.subspan(size);
    }
#else
    require(EVP_DigestUpdate(impl_->context, bytes.data(), bytes.size()) == 1, ErrorCode::io, "SHA-256 update failed");
#endif
}
Digest Sha256::finish() {
    require(!impl_->finished, ErrorCode::invalid_argument, "Digest already finalized");
    Digest result{};
#ifdef _WIN32
    require(BCryptFinishHash(impl_->hash, result.data(), static_cast<ULONG>(result.size()), 0) >= 0,
        ErrorCode::io, "SHA-256 finalization failed");
#else
    unsigned length = 0;
    require(EVP_DigestFinal_ex(impl_->context, result.data(), &length) == 1 && length == result.size(),
        ErrorCode::io, "SHA-256 finalization failed");
#endif
    impl_->finished = true;
    return result;
}
Digest sha256(std::span<const std::uint8_t> bytes) { Sha256 hash; hash.update(bytes); return hash.finish(); }
std::string hex(const Digest& digest) {
    constexpr char digits[] = "0123456789abcdef";
    std::string result; result.reserve(64);
    for (auto b : digest) { result.push_back(digits[b >> 4]); result.push_back(digits[b & 15]); }
    return result;
}
}
