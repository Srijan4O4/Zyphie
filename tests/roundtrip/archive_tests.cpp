#include "zyphie/archive.hpp"
#include "zyphie/time.hpp"
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
        if (item.is_regular_file()) { EXPECT_EQ(read(item.path()),read(output/relative)) << relative; }
        const auto before=std::chrono::floor<std::chrono::seconds>(to_system_time(fs::last_write_time(item.path())));
        const auto after=std::chrono::floor<std::chrono::seconds>(to_system_time(fs::last_write_time(output/relative)));
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
    fs::last_write_time(source/"random.bin",to_file_time(std::chrono::sys_seconds{std::chrono::seconds{1700000000}}));
    PackOptions options; options.store=GetParam(); options.block_size=16384;
    const auto archive=w.root/"roundtrip.zyph";
    ArchiveWriter::pack({source},archive,options);
    ArchiveReader reader(archive); EXPECT_EQ(reader.entries().size(),7);
    EXPECT_GT(reader.block_count(),10); if (GetParam()) { EXPECT_EQ(reader.zstandard_blocks(),0); } else { EXPECT_GT(reader.zstandard_blocks(),0); }
    reader.verify(); reader.extract(w.root/"output"); compare_tree(source,w.root/"output"/source.filename());
    EXPECT_EQ(read(source/"repeat.bin"),repeated); EXPECT_EQ(read(source/"random.bin"),random_bytes);
}
INSTANTIATE_TEST_SUITE_P(Codecs,RoundTrip,::testing::Values(true,false));

TEST(Archive, ReadsIndependentGoldenArchives) {
    Workspace w;
    for (const auto* name:{"empty.zyph","empty-file.zyph","empty-directory.zyph","store-abc.zyph","zstd-rle.zyph"}) {
        ArchiveReader reader(fs::path(ZYPH_FIXTURES)/name); reader.verify();
        const auto destination=w.root/name; reader.extract(destination);
        if (std::string_view(name)=="empty.zyph") { EXPECT_TRUE(fs::is_empty(destination)); }
        if (std::string_view(name)=="empty-file.zyph") { EXPECT_EQ(fs::file_size(destination/"empty.txt"),0); }
        if (std::string_view(name)=="empty-directory.zyph") { EXPECT_TRUE(fs::is_directory(destination/"empty-dir")); }
        if (std::string_view(name)=="store-abc.zyph") { EXPECT_EQ(read(destination/"abc.txt"),(binary::Bytes{'a','b','c'})); }
        if (std::string_view(name)=="zstd-rle.zyph") { EXPECT_EQ(read(destination/"repeat.txt"),binary::Bytes(64,'a')); }
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
    fs::last_write_time(w.root/"abc.txt",to_file_time(std::chrono::sys_seconds{std::chrono::seconds{1700000000}}));
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


TEST(Timestamp, PreservesUnixSecondsAtSupportedBoundaries) {
    for (const auto seconds : {-2147483648LL, -1LL, 0LL, 1700000000LL, 4102444800LL}) {
        const auto system=std::chrono::sys_seconds{std::chrono::seconds{seconds}};
        EXPECT_EQ(std::chrono::floor<std::chrono::seconds>(to_system_time(to_file_time(system))),system);
    }
}

#include "filesystem/io.hpp"
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <aclapi.h>
#else
#include <sys/stat.h>
#endif
namespace {
struct HookReset { ~HookReset() { io::test_hook={}; } };
void no_staging(const fs::path& root) {
    for (const auto& item:fs::directory_iterator(root)) { EXPECT_FALSE(item.path().filename().string().starts_with(".zyph-part-")); }
}
void error_code(const std::function<void()>& operation,ErrorCode expected) {
    try { operation(); FAIL() << "Expected error"; }
    catch(const Error& e) { EXPECT_EQ(e.code(),expected) << e.what(); }
}
}
TEST(Security, PackingFaultsPreserveSourcesAndCleanPartialOutputs) {
    for(auto failure:{io::Event::write,io::Event::flush,io::Event::publish}) {
        Workspace w; HookReset reset; write(w.root/"source",binary::Bytes(4096,'s')); int writes=0;
        io::test_hook=[&](io::Event e) { if(e==failure && (e!=io::Event::write || ++writes==2)) throw Error(ErrorCode::io,"Injected disk/flush/publish failure"); };
        error_code([&]{ ArchiveWriter::pack({w.root/"source"},w.root/"archive"); },ErrorCode::io);
        io::test_hook={}; EXPECT_EQ(read(w.root/"source"),binary::Bytes(4096,'s')); EXPECT_FALSE(fs::exists(w.root/"archive")); no_staging(w.root);
    }
}
TEST(Security, ExtractionFaultsAreTransactional) {
    for(auto failure:{io::Event::write,io::Event::flush,io::Event::publish,io::Event::mkdir}) {
        Workspace w; HookReset reset; ArchiveReader r(fs::path(ZYPH_FIXTURES)/"store-abc.zyph");
        io::test_hook=[&](io::Event e) { if(e==failure) throw Error(ErrorCode::io,"Injected failure"); };
        error_code([&]{r.extract(w.root/"out");},ErrorCode::io);
        io::test_hook={}; EXPECT_FALSE(fs::exists(w.root/"out")); no_staging(w.root);
    }
}
TEST(Security, InFlightCancellationCleansOutputs) {
    Workspace w; HookReset reset; write(w.root/"source",binary::Bytes(4096,'x'));
    PackOptions options; options.store=true; options.block_size=256; std::stop_source stop; options.cancellation=stop.get_token(); int writes=0;
    io::test_hook=[&](io::Event e){if(e==io::Event::write && ++writes==2) stop.request_stop();};
    error_code([&]{ArchiveWriter::pack({w.root/"source"},w.root/"archive",options);},ErrorCode::cancelled);
    io::test_hook={}; EXPECT_FALSE(fs::exists(w.root/"archive")); no_staging(w.root);
    std::stop_source extraction; ArchiveReader r(fs::path(ZYPH_FIXTURES)/"store-abc.zyph");
    io::test_hook=[&](io::Event e){if(e==io::Event::write) extraction.request_stop();};
    error_code([&]{r.extract(w.root/"out",extraction.get_token());},ErrorCode::cancelled);
    io::test_hook={}; EXPECT_FALSE(fs::exists(w.root/"out")); no_staging(w.root);
}
TEST(Security, PublicationRaceNeverReplacesWinner) {
    Workspace w; HookReset reset; ArchiveReader r(fs::path(ZYPH_FIXTURES)/"store-abc.zyph");
    io::test_hook=[&](io::Event e){if(e==io::Event::publish){fs::create_directory(w.root/"out");write(w.root/"out"/"sentinel",{42});}};
    EXPECT_THROW(r.extract(w.root/"out"),Error); io::test_hook={};
    EXPECT_EQ(read(w.root/"out"/"sentinel"),binary::Bytes{42}); no_staging(w.root);
}
TEST(Security, AncestorSwapCannotRedirectCreation) {
    Workspace w; HookReset reset; fs::create_directory(w.root/"parent"); fs::create_directory(w.root/"outside");
    bool attempted=false; std::error_code ec;
    io::test_hook=[&](io::Event e){if(e!=io::Event::mkdir || attempted)return; attempted=true;
        fs::rename(w.root/"parent",w.root/"moved",ec); if(!ec) fs::create_directory_symlink(w.root/"outside",w.root/"parent");};
    EXPECT_TRUE(io::create_directory(w.root/"parent"/"child")); io::test_hook={};
    EXPECT_TRUE(attempted); EXPECT_FALSE(fs::exists(w.root/"outside"/"child"));
#ifdef _WIN32
    EXPECT_TRUE(ec); EXPECT_TRUE(fs::exists(w.root/"parent"/"child"));
#else
    EXPECT_FALSE(ec); EXPECT_TRUE(fs::exists(w.root/"moved"/"child"));
#endif
}
TEST(Security, NativeMutationsAndCleanupDoNotFollowLinks) {
    Workspace w; fs::create_directory(w.root/"outside"); write(w.root/"outside"/"sentinel",{42});fs::create_directory(w.root/"stage");
    std::error_code ec;fs::create_directory_symlink(w.root/"outside",w.root/"stage"/"link",ec);
    if(ec) GTEST_SKIP()<<"Symlink privilege unavailable: "<<ec.message();
    EXPECT_THROW(io::File(w.root/"stage"/"link"/"bad",true),Error);
    EXPECT_THROW(io::create_directory(w.root/"stage"/"link"/"bad"),Error);
    EXPECT_THROW(io::set_time(w.root/"stage"/"link"/"sentinel",0),Error);
    {io::Temporary cleanup;cleanup.path=w.root/"stage";cleanup.directory=true;cleanup.active=true;}
    EXPECT_EQ(read(w.root/"outside"/"sentinel"),binary::Bytes{42});EXPECT_FALSE(fs::exists(w.root/"stage"));
}
TEST(Security, PrivateDirectoryPermissionsAreAppliedAtCreation) {
    Workspace w;ASSERT_TRUE(io::create_directory(w.root/"private"));
#ifdef _WIN32
    PSECURITY_DESCRIPTOR descriptor=nullptr;PACL acl=nullptr;
    ASSERT_EQ(GetNamedSecurityInfoW((w.root/"private").c_str(),SE_FILE_OBJECT,DACL_SECURITY_INFORMATION,nullptr,nullptr,&acl,nullptr,&descriptor),ERROR_SUCCESS);
    SECURITY_DESCRIPTOR_CONTROL control{};DWORD revision=0;
    EXPECT_TRUE(GetSecurityDescriptorControl(descriptor,&control,&revision));EXPECT_NE(control & SE_DACL_PROTECTED,0);EXPECT_NE(acl,nullptr);
    if(acl){EXPECT_EQ(acl->AceCount,2);} LocalFree(descriptor);
#else
    struct stat st{};ASSERT_EQ(::stat((w.root/"private").c_str(),&st),0);EXPECT_EQ(st.st_mode & 0777,0700);
#endif
}
TEST(Security, SourceMutationIsDetectedBeforePublication) {
    Workspace w;HookReset reset;write(w.root/"source",{1,2});int writes=0;
    io::test_hook=[&](io::Event e){if(e==io::Event::write && ++writes==1)write(w.root/"source",{3,4,5});};
    error_code([&]{ArchiveWriter::pack({w.root/"source"},w.root/"archive");},ErrorCode::source_changed);
    io::test_hook={};EXPECT_EQ(read(w.root/"source"),(binary::Bytes{3,4,5}));EXPECT_FALSE(fs::exists(w.root/"archive"));no_staging(w.root);
}
TEST(Security, NativeCaseCollisionFailsWithoutPartialPublication) {
    Workspace w;fs::create_directory(w.root/"source");write(w.root/"source"/"A",{});write(w.root/"source"/"b",{});
    PackOptions options;options.store=true;ArchiveWriter::pack({w.root/"source"},w.root/"archive",options);
    auto bytes=read(w.root/"archive");const std::string path="source/b";
    auto at=std::search(bytes.begin(),bytes.end(),path.begin(),path.end());ASSERT_NE(at,bytes.end());*(at+7)='a';refresh_metadata(bytes);write(w.root/"forged",bytes);
    ArchiveReader r(w.root/"forged");
#ifdef _WIN32
    EXPECT_THROW(r.extract(w.root/"out"),Error);EXPECT_FALSE(fs::exists(w.root/"out"));no_staging(w.root);
#else
    r.extract(w.root/"out");EXPECT_TRUE(fs::exists(w.root/"out"/"source"/"A"));EXPECT_TRUE(fs::exists(w.root/"out"/"source"/"a"));
#endif
}
