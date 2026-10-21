#include <gtest/gtest.h>
#include <zstd.h>

#include <array>
#include <cstddef>
#include <filesystem>
#include <span>
#include <stop_token>
#include <thread>
#include <vector>

// A toolchain gate: require C++20 library facilities used by the future engine.
TEST(Toolchain, SupportsRequiredStandardLibraryFacilities) {
    const std::array<int, 3> values{1, 2, 3};
    const std::span<const int> view{values};
    EXPECT_EQ(view.subspan(1).front(), 2);
    EXPECT_EQ((std::filesystem::path{"source"} / "file").filename(), "file");
    std::stop_source cancellation;
    auto token = cancellation.get_token();
    EXPECT_FALSE(token.stop_requested());
    EXPECT_TRUE(cancellation.request_stop());
    std::jthread worker{[token] { EXPECT_TRUE(token.stop_requested()); }};
}

// Dependency integration only. This does not validate any .zyph archive behavior.
TEST(Dependencies, ZstandardRoundTripsBinaryBytes) {
    std::vector<std::byte> original(8192);
    for (std::size_t i = 0; i < original.size(); ++i) {
        original[i] = static_cast<std::byte>(i % 256);
    }
    std::vector<std::byte> compressed(ZSTD_compressBound(original.size()));
    const auto compressed_size = ZSTD_compress(compressed.data(), compressed.size(),
        original.data(), original.size(), 3);
    ASSERT_FALSE(ZSTD_isError(compressed_size)) << ZSTD_getErrorName(compressed_size);
    std::vector<std::byte> restored(original.size());
    const auto restored_size = ZSTD_decompress(restored.data(), restored.size(),
        compressed.data(), compressed_size);
    ASSERT_FALSE(ZSTD_isError(restored_size)) << ZSTD_getErrorName(restored_size);
    EXPECT_EQ(restored_size, original.size());
    EXPECT_EQ(restored, original);
}
