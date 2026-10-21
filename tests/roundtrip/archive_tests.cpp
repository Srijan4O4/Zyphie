#include "zyphie/archive.hpp"
#include <gtest/gtest.h>
#include <algorithm>
#include <chrono>
#include <fstream>
#include <random>
#include <set>

using namespace zyphie;
namespace fs=std::filesystem;
namespace {
struct Workspace {
    fs::path root;
    Workspace() {
        std::random_device random;
        for (int i=0;i<16;++i) {
            root=fs::temp_directory_path() / ("zyph-test-"+std::to_string(random())+std::to_string(random()));
            if (fs::create_directory(root)) return;
        }
        throw std::runtime_error("Cannot create test workspace");
    }
    ~Workspace() { std::error_code ec; fs::remove_all(root,ec); }
};
void write(const fs::path& path,const binary::Bytes& bytes) {
    std::ofstream out(path,std::ios::binary|std::ios::trunc);
    out.write(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));
    if (!out) throw std::runtime_error("Fixture write failed");
}
binary::Bytes read(const fs::path& path) {
    std::ifstream in(path,std::ios::binary);
    if (!in) throw std::runtime_error("Fixture read failed");
    return {std::istreambuf_iterator<char>(in),std::istreambuf_iterator<char>{}};
}
void refresh_metadata(binary::Bytes& bytes) {
    auto f=format::read_footer(std::span(bytes).last(128),bytes.size());
    Sha256 hash; hash.update(std::span(bytes).first(32));
    hash.update(std::span(bytes).subspan(static_cast<std::size_t>(f.manifest_offset),static_cast<std::size_t>(f.manifest_size+f.index_size)));
    f.metadata_digest=hash.finish(); auto footer=format::footer(f);
    std::copy(footer.begin(),footer.end(),bytes.end()-128);
}
void compare_tree(const fs::path& input,const fs::path& output) {
    std::set<fs::path> expected,actual;
    for (const auto& item:fs::recursive_directory_iterator(input)) {
        const auto relative=item.path().lexically_relative(input); expected.insert(relative);
        ASSERT_TRUE(fs::exists(output/relative));
        EXPECT_EQ(item.is_directory(),fs::is_directory(output/relative));
        if (item.is_regular_file()) EXPECT_EQ(read(item.path()),read(output/relative)) << relative;
        const auto before=std::chrono::floor<std::chrono::seconds>(fs::file_time_type::clock::to_sys(fs::last_write_time(item.path())));
        const auto after=std::chrono::floor<std::chrono::seconds>(fs::file_time_type::clock::to_sys(fs::last_write_time(output/relative)));
        EXPECT_EQ(before,after) << relative;
    }
    for (const auto& item:fs::recursive_directory_iterator(output)) actual.insert(item.path().lexically_relative(output));
    EXPECT_EQ(expected,actual);
}
}
class RoundTrip : public ::testing::TestWithParam<bool> {};
TEST_P(RoundTrip, MixedNestedUnicodeEmptyAndMultiBlockData) {
    Workspace w; const auto source=w.root/fs::path(u8"source 文件");
    fs::create_directories(source/"nested"/"empty dir");
    write(source/"empty.bin",{}); write(source/fs::path(u8"nested/हेलो 世界.txt"),binary::Bytes{'h',0,'i',255});
    binary::Bytes repeated(150000,0x41); write(source/"repeat.bin",repeated);
    std::mt19937 random(42); binary::Bytes random_bytes(90000);
    for (auto& b:random_bytes) b=static_cast<std::uint8_t>(random() & 255);
    write(source/"random.bin",random_bytes);
    fs::last_write_time(source/"random.bin",fs::file_time_type::clock::from_sys(std::chrono::sys_seconds{std::chrono::seconds{1700000000}}));
    PackOptions options; options.store=GetParam(); options.block_size=16384;
    const auto archive=w.root/"roundtrip.zyph";
    ArchiveWriter::pack({source},archive,options);
    ArchiveReader reader(archive); EXPECT_EQ(reader.entries().size(),7);
    EXPECT_GT(reader.block_count(),10); if (GetParam()) EXPECT_EQ(reader.zstandard_blocks(),0); else EXPECT_GT(reader.zstandard_blocks(),0);
    reader.verify(); reader.extract(w.root/"output"); compare_tree(source,w.root/"output"/source.filename());
    EXPECT_EQ(read(source/"repeat.bin"),repeated); EXPECT_EQ(read(source/"random.bin"),random_bytes);
}
INSTANTIATE_TEST_SUITE_P(Codecs,RoundTrip,::testing::Values(true,false));

TEST(Archive, ReadsIndependentGoldenArchives) {
    Workspace w;
    for (const auto* name:{"empty.zyph","empty-file.zyph","empty-directory.zyph","store-abc.zyph","zstd-rle.zyph"}) {
        ArchiveReader reader(fs::path(ZYPH_FIXTURES)/name); reader.verify();
        const auto destination=w.root/name; reader.extract(destination);
        if (std::string_view(name)=="empty.zyph") EXPECT_TRUE(fs::is_empty(destination));
        if (std::string_view(name)=="empty-file.zyph") EXPECT_EQ(fs::file_size(destination/"empty.txt"),0);
        if (std::string_view(name)=="empty-directory.zyph") EXPECT_TRUE(fs::is_directory(destination/"empty-dir"));
        if (std::string_view(name)=="store-abc.zyph") EXPECT_EQ(read(destination/"abc.txt"),(binary::Bytes{'a','b','c'}));
        if (std::string_view(name)=="zstd-rle.zyph") EXPECT_EQ(read(destination/"repeat.txt"),binary::Bytes(64,'a'));
    }
}
TEST(Archive, MultipleFilesAndStoreFallback) {
    Workspace w; write(w.root/"a",{0}); write(w.root/"b",{1});
    ArchiveWriter::pack({w.root/"b",w.root/"a"},w.root/"x.zyph");
    ArchiveReader r(w.root/"x.zyph"); ASSERT_EQ(r.entries().size(),2);
    EXPECT_EQ(r.entries()[0].path,"a"); EXPECT_EQ(r.zstandard_blocks(),0);
    r.extract(w.root/"out"); EXPECT_EQ(read(w.root/"out"/"b"),binary::Bytes{1});
}
TEST(Archive, StreamsBeyondDefaultBlockSize) {
    Workspace w; binary::Bytes content(17*1024*1024,17); write(w.root/"large",content);
    ArchiveWriter::pack({w.root/"large"},w.root/"large.zyph");
    ArchiveReader r(w.root/"large.zyph"); EXPECT_EQ(r.block_count(),5);
    r.extract(w.root/"out"); EXPECT_EQ(read(w.root/"out"/"large"),content);
}
TEST(Archive, ListingDoesNotDecodeCorruptPayloadButVerificationAndExtractionDo) {
    Workspace w; auto bytes=read(fs::path(ZYPH_FIXTURES)/"store-abc.zyph"); bytes[32]^=1;
    write(w.root/"bad.zyph",bytes); ArchiveReader r(w.root/"bad.zyph");
    ASSERT_EQ(r.entries().size(),1); EXPECT_EQ(r.entries()[0].path,"abc.txt");
    EXPECT_THROW(r.verify(),Error);
    EXPECT_THROW(r.extract(w.root/"out"),Error);
    EXPECT_FALSE(fs::exists(w.root/"out"));
    for (const auto& item:fs::directory_iterator(w.root)) EXPECT_FALSE(item.path().filename().string().starts_with(".zyph-part-"));
}
TEST(Archive, RejectsEveryTruncationAndTrailingData) {
    Workspace w; const auto bytes=read(fs::path(ZYPH_FIXTURES)/"store-abc.zyph");
    for (std::size_t size=0;size<bytes.size();++size) {
        write(w.root/"cut.zyph",binary::Bytes(bytes.begin(),bytes.begin()+static_cast<std::ptrdiff_t>(size)));
        EXPECT_THROW(ArchiveReader{w.root/"cut.zyph"},Error) << size;
    }
    auto trailing=bytes; trailing.push_back(0); write(w.root/"cut.zyph",trailing);
    EXPECT_THROW(ArchiveReader{w.root/"cut.zyph"},Error);
}
TEST(Archive, RejectsForgedPathsCodecsBoundsAndWholeFileDigest) {
    Workspace w; const auto original=read(fs::path(ZYPH_FIXTURES)/"store-abc.zyph");
    const auto f=format::read_footer(std::span(original).last(128),original.size());
    const auto mo=static_cast<std::size_t>(f.manifest_offset), ix=static_cast<std::size_t>(f.index_offset);
    auto check=[&](binary::Bytes bytes) {
        refresh_metadata(bytes); write(w.root/"bad.zyph",bytes);
        EXPECT_THROW(ArchiveReader{w.root/"bad.zyph"},Error);
    };
    auto bytes=original; const std::string traversal="../evil";
    std::copy(traversal.begin(),traversal.end(),bytes.begin()+static_cast<std::ptrdiff_t>(mo+16+72)); check(bytes);
    bytes=original; bytes[ix+16+16]=17; check(bytes);
    bytes=original; bytes[ix+16]=33; check(bytes);
    bytes=original; bytes[mo+16+32]=2; check(bytes);
    bytes=original; bytes[mo+16+40]^=1; refresh_metadata(bytes); write(w.root/"bad.zyph",bytes);
    ArchiveReader reader(w.root/"bad.zyph"); EXPECT_THROW(reader.verify(),Error);
}
TEST(Archive, EnforcesResourceLimitsBeforeOutput) {
    const auto fixture=fs::path(ZYPH_FIXTURES)/"store-abc.zyph";
    Limits limits; limits.metadata_bytes=16; EXPECT_THROW((ArchiveReader{fixture,limits}),Error);
    limits=Limits{}; limits.entries=0; EXPECT_THROW((ArchiveReader{fixture,limits}),Error);
    limits=Limits{}; limits.blocks=0; EXPECT_THROW((ArchiveReader{fixture,limits}),Error);
    limits=Limits{}; limits.file_bytes=2; EXPECT_THROW((ArchiveReader{fixture,limits}),Error);
    limits=Limits{}; limits.total_bytes=2; EXPECT_THROW((ArchiveReader{fixture,limits}),Error);
    Workspace w; write(w.root/"source",binary::Bytes(100,'x'));
    PackOptions options; options.limits.metadata_bytes=40;
    EXPECT_THROW(ArchiveWriter::pack({w.root/"source"},w.root/"out.zyph",options),Error);
    EXPECT_FALSE(fs::exists(w.root/"out.zyph"));
}
TEST(Archive, NeverOverwritesOrIncludesDestination) {
    Workspace w; fs::create_directory(w.root/"source"); write(w.root/"source"/"a",{1,2}); write(w.root/"existing.zyph",{99});
    EXPECT_THROW(ArchiveWriter::pack({w.root/"source"},w.root/"existing.zyph"),Error);
    EXPECT_EQ(read(w.root/"existing.zyph"),binary::Bytes{99});
    EXPECT_THROW(ArchiveWriter::pack({w.root/"source"},w.root/"source"/"self.zyph"),Error);
    ArchiveReader fixture(fs::path(ZYPH_FIXTURES)/"store-abc.zyph");
    EXPECT_THROW(fixture.extract(w.root/"source"),Error);
    EXPECT_EQ(read(w.root/"source"/"a"),(binary::Bytes{1,2}));
    EXPECT_THROW(ArchiveWriter::pack({w.root/"source"/"a",w.root/"source"/"a"},w.root/"duplicate.zyph"),Error);
}
TEST(Archive, CancellationLeavesNoFinalOutputs) {
    Workspace w; write(w.root/"source",{1,2,3}); std::stop_source stop; stop.request_stop();
    PackOptions options; options.cancellation=stop.get_token();
    EXPECT_THROW(ArchiveWriter::pack({w.root/"source"},w.root/"out.zyph",options),Error);
    EXPECT_FALSE(fs::exists(w.root/"out.zyph"));
    ArchiveReader fixture(fs::path(ZYPH_FIXTURES)/"store-abc.zyph");
    EXPECT_THROW(fixture.extract(w.root/"out",stop.get_token()),Error);
    EXPECT_FALSE(fs::exists(w.root/"out"));
}
TEST(Archive, RejectsSymlinkSourcesAndDestinations) {
    Workspace w; fs::create_directory(w.root/"source"); write(w.root/"source"/"a",{1});
    std::error_code error; fs::create_directory_symlink(w.root/"source",w.root/"link",error);
    if (error) GTEST_SKIP() << "Symlink creation unavailable: " << error.message();
    EXPECT_THROW(ArchiveWriter::pack({w.root/"link"},w.root/"x.zyph"),Error);
    EXPECT_THROW(ArchiveWriter::pack({w.root/"source"/"a"},w.root/"link"/"x.zyph"),Error);
}

TEST(Archive, StoreWriterExactlyMatchesIndependentReferenceBytes) {
    Workspace w; write(w.root/"abc.txt",{'a','b','c'});
    fs::last_write_time(w.root/"abc.txt",fs::file_time_type::clock::from_sys(std::chrono::sys_seconds{std::chrono::seconds{1700000000}}));
    PackOptions options; options.store=true;
    ArchiveWriter::pack({w.root/"abc.txt"},w.root/"actual.zyph",options);
    EXPECT_EQ(read(w.root/"actual.zyph"),read(fs::path(ZYPH_FIXTURES)/"store-timestamp.zyph"));
}
TEST(Archive, LatePackingFailureCleansOwnedTemporaryOutput) {
    Workspace w; write(w.root/"source",binary::Bytes(100,'x'));
    PackOptions options; options.block_size=1; options.limits.blocks=1;
    EXPECT_THROW(ArchiveWriter::pack({w.root/"source"},w.root/"out.zyph",options),Error);
    EXPECT_FALSE(fs::exists(w.root/"out.zyph"));
    EXPECT_EQ(read(w.root/"source"),binary::Bytes(100,'x'));
    for (const auto& item:fs::directory_iterator(w.root)) EXPECT_EQ(item.path().filename(),"source");
}
