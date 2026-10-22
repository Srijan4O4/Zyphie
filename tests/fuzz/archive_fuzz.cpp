#include "zyphie/archive.hpp"
#include <fstream>
#include <random>
#include <cstdlib>
#include <sys/stat.h>
#include <unistd.h>
namespace fs=std::filesystem;
using namespace zyphie;
namespace {
struct Workspace {
    fs::path root;
    Workspace() {
        std::string pattern=(fs::temp_directory_path()/"zyph-fuzz-XXXXXX").string();
        if(!::mkdtemp(pattern.data())) std::abort(); root=pattern;
    }
    ~Workspace(){std::error_code ec;fs::remove_all(root,ec);}
};
void exercise(const binary::Bytes& bytes,const fs::path& path) {
    {std::ofstream f(path,std::ios::binary|std::ios::trunc);f.write(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));if(!f)std::abort();}
    Limits limits;limits.entries=64;limits.blocks=128;limits.metadata_bytes=65536;
    limits.archive_bytes=1048576;limits.file_bytes=1048576;limits.total_bytes=1048576;
    try{ArchiveReader reader(path,limits);reader.verify();}catch(const Error&){}
}
}
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data,std::size_t size) {
    if(size>1048576)return 0;
    static Workspace workspace;
    binary::Bytes bytes(data,data+size);exercise(bytes,workspace.root/"input");
    // Repair only bounded section digests so mutations also reach deep manifest/index checks.
    if(size>=192) {
        try {
            auto footer_hash=sha256(std::span(bytes).subspan(size-128,96));
            std::copy(footer_hash.begin(),footer_hash.end(),bytes.end()-32);
            auto f=format::read_footer(std::span(bytes).last(128),size);
            Sha256 h;h.update(std::span(bytes).first(32));
            h.update(std::span(bytes).subspan(static_cast<std::size_t>(f.manifest_offset),static_cast<std::size_t>(f.manifest_size+f.index_size)));
            f.metadata_digest=h.finish();auto footer=format::footer(f);std::copy(footer.begin(),footer.end(),bytes.end()-128);
            exercise(bytes,workspace.root/"input");
        }catch(const Error&){}
    }
    return 0;
}
