#include "zyphie/format.hpp"
#include <gtest/gtest.h>
#include <limits>
#include <fstream>
#include <filesystem>
using namespace zyphie;
TEST(Binary, ExactLittleEndianAndBounds) {
    binary::Encoder e; e.integer(0x0123456789abcdefULL, 8);
    EXPECT_EQ(e.data, (binary::Bytes{0xef,0xcd,0xab,0x89,0x67,0x45,0x23,0x01}));
    binary::Decoder d(e.data);
    EXPECT_EQ(d.integer(8), 0x0123456789abcdefULL); d.end();
    EXPECT_THROW(d.integer(1), Error);
    EXPECT_THROW(e.integer(256, 1), Error);
    EXPECT_THROW(binary::add(UINT64_MAX, 1), Error);
    EXPECT_THROW(binary::multiply(UINT64_MAX, 2), Error);
    EXPECT_EQ(binary::add(UINT64_MAX, 0), UINT64_MAX);
    EXPECT_EQ(binary::multiply(UINT64_MAX, 0), 0);
}
TEST(Digest, StandardVectorsAndIncremental) {
    EXPECT_EQ(hex(sha256({})), "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    Sha256 h; h.update(binary::Bytes{'a'}); h.update(binary::Bytes{'b','c'});
    EXPECT_EQ(hex(h.finish()), "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    EXPECT_THROW(h.finish(), Error);
}
TEST(Format, HeaderVectorAndEveryTruncation) {
    const binary::Bytes expected{0x5a,0x59,0x50,0x48,13,10,26,10,1,0,0,0,32,0,0,0,0,0,0,0,1,0,0,0,0,0,64,0,0,0,0,0};
    EXPECT_EQ(format::header(format::default_block), expected);
    EXPECT_EQ(format::read_header(expected), format::default_block);
    for (std::size_t i=0; i<expected.size(); ++i) EXPECT_THROW(format::read_header(std::span(expected).first(i)), Error);
    auto bad = expected; bad[10] = 1;
    EXPECT_THROW(format::read_header(bad), Error);
    bad = expected; bad[28] = 1;
    EXPECT_THROW(format::read_header(bad), Error);
}
TEST(Format, FooterIntegrityAndOverflow) {
    format::Footer f{32,16,48,16,192,{}};
    auto bytes = format::footer(f);
    EXPECT_EQ(bytes.size(),128);
    EXPECT_EQ(format::read_footer(bytes,192).manifest_offset,32);
    for (std::size_t i=0;i<bytes.size();++i) { auto bad=bytes; bad[i]^=1;
    EXPECT_THROW(format::read_footer(bad,192),Error); }
    f.index_offset=UINT64_MAX;
    EXPECT_THROW(format::read_footer(format::footer(f),192),Error);
}
TEST(Paths, RejectsTraversalDevicesAliasesAndMalformedUtf8) {
    for (auto p : {"../a","/a","a//b","a/","C:/a","a\\b","a:stream","con.txt","LPT9","x.","x ","a/../b","CONIN$"})
        EXPECT_THROW(format::validate_path(p),Error) << p;
    EXPECT_THROW(format::validate_path(std::string("x\0y",3)),Error);
    EXPECT_THROW(format::validate_path("\xc0\xaf"),Error);
    EXPECT_THROW(format::validate_path("\xed\xa0\x80"),Error);
    EXPECT_THROW(format::validate_path("\xf4\x90\x80\x80"),Error);
    EXPECT_THROW(format::validate_path("COM\xc2\xb9.txt"),Error);
    EXPECT_NO_THROW(format::validate_path("folder/hello world.txt"));
    EXPECT_NO_THROW(format::validate_path("\xe6\x96\x87\xe4\xbb\xb6.txt"));
}

TEST(Format, IndependentReferenceMetadata) {
    for (const auto* name : {"empty.zyph", "empty-file.zyph", "empty-directory.zyph", "store-abc.zyph", "zstd-rle.zyph"}) {
        std::ifstream in(std::filesystem::path(ZYPH_FIXTURES) / name, std::ios::binary);
        ASSERT_TRUE(in.good());
        const binary::Bytes data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>{});
        ASSERT_GE(data.size(), 192);
        const auto bytes = std::span<const std::uint8_t>(data);
        EXPECT_EQ(format::read_header(bytes.first(32)), format::default_block);
        const auto f = format::read_footer(bytes.last(128), data.size());
        Sha256 hash; hash.update(bytes.first(32));
        hash.update(bytes.subspan(static_cast<std::size_t>(f.manifest_offset), static_cast<std::size_t>(f.manifest_size)));
        hash.update(bytes.subspan(static_cast<std::size_t>(f.index_offset), static_cast<std::size_t>(f.index_size)));
        EXPECT_EQ(hash.finish(), f.metadata_digest) << name;
    }
}
