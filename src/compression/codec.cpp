#include "compression/codec.hpp"
#include <zstd.h>
#include <memory>
namespace zyphie::codec {
binary::Bytes compress(std::span<const std::uint8_t> bytes, int level) {
    std::unique_ptr<ZSTD_CCtx, decltype(&ZSTD_freeCCtx)> context(ZSTD_createCCtx(), ZSTD_freeCCtx);
    require(context != nullptr, ErrorCode::resource_limit, "Cannot allocate compression context");
    require(!ZSTD_isError(ZSTD_CCtx_setParameter(context.get(), ZSTD_c_compressionLevel, level)) &&
        !ZSTD_isError(ZSTD_CCtx_setParameter(context.get(), ZSTD_c_windowLog, 24)),
        ErrorCode::invalid_argument, "Invalid compression settings");
    binary::Bytes result(ZSTD_compressBound(bytes.size()));
    auto size = ZSTD_compress2(context.get(), result.data(), result.size(), bytes.data(), bytes.size());
    require(!ZSTD_isError(size), ErrorCode::io, "Compression failed"); result.resize(size); return result;
}
binary::Bytes decompress(std::span<const std::uint8_t> bytes, std::uint32_t original_size) {
    require(ZSTD_getFrameContentSize(bytes.data(), bytes.size()) == original_size &&
        ZSTD_findFrameCompressedSize(bytes.data(), bytes.size()) == bytes.size() &&
        ZSTD_getDictID_fromFrame(bytes.data(), bytes.size()) == 0,
        ErrorCode::corrupt_archive, "Invalid Zstandard frame or declared size");
    std::unique_ptr<ZSTD_DCtx, decltype(&ZSTD_freeDCtx)> context(ZSTD_createDCtx(), ZSTD_freeDCtx);
    require(context != nullptr, ErrorCode::resource_limit, "Cannot allocate decompression context");
    require(!ZSTD_isError(ZSTD_DCtx_setParameter(context.get(), ZSTD_d_windowLogMax, 24)),
        ErrorCode::io, "Cannot set decoder window limit");
    binary::Bytes result(original_size);
    auto size = ZSTD_decompressDCtx(context.get(), result.data(), result.size(), bytes.data(), bytes.size());
    require(!ZSTD_isError(size) && size == original_size, ErrorCode::corrupt_archive, "Zstandard decompression failed");
    return result;
}
}
