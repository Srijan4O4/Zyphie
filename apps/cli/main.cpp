#include "zyphie/archive.hpp"
#include "zyphie/version.hpp"
#include <charconv>
#include <iostream>
#include <string_view>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif

namespace {
std::filesystem::path native(std::string_view text) {
    return std::filesystem::path(std::u8string(reinterpret_cast<const char8_t*>(text.data()),text.size()));
}
void usage() {
    std::cout << "Zyphie " << zyphie::version() << "\n"
        "Usage:\n"
        "  zyph pack <source>... -o <archive.zyph> [--store] [--preset fast|balanced|maximum] [--block-size bytes]\n"
        "  zyph extract <archive.zyph> -o <new-directory>\n"
        "  zyph list <archive.zyph>\n"
        "  zyph verify <archive.zyph>\n"
        "  zyph info <archive.zyph>\n"
        "  zyph --help | --version\n"
        "Destinations must not exist. Inputs are preserved. Use -- before source names beginning with -.\n"
        "Milestone 2: full extraction only; overwrite, threads, progress and GUI are not implemented.\n";
}
int run(const std::vector<std::string>& args) {
    using namespace zyphie;
    try {
        require(args.size() >= 2,ErrorCode::invalid_argument,"A command is required; use --help");
        const auto& command=args[1];
        if (command=="--help" && args.size()==2) { usage(); return 0; }
        if (command=="--version" && args.size()==2) { std::cout << "Zyphie " << version() << '\n'; return 0; }
        require(command=="pack" || command=="extract" || command=="list" || command=="verify" || command=="info",
            ErrorCode::invalid_argument,"Unknown command; use --help");
        PackOptions options; std::filesystem::path output; std::vector<std::filesystem::path> inputs;
        bool literal=false;
        for (std::size_t i=2;i<args.size();++i) {
            const auto& arg=args[i];
            auto value=[&]() -> const std::string& {
                require(i+1<args.size(),ErrorCode::invalid_argument,"Missing value for " + arg); return args[++i];
            };
            if (!literal && arg=="--") { literal=true; continue; }
            if (!literal && arg=="-o") {
                require(output.empty() && (command=="pack" || command=="extract"),ErrorCode::invalid_argument,"Unexpected or repeated -o");
                output=native(value());
            } else if (!literal && arg=="--store" && command=="pack") { options.store=true; }
            else if (!literal && arg=="--preset" && command=="pack") {
                const auto& preset=value();
                require(preset=="fast" || preset=="balanced" || preset=="maximum",ErrorCode::invalid_argument,"Unknown preset");
                options.compression_level=preset=="fast"?1:preset=="balanced"?3:19;
            } else if (!literal && arg=="--block-size" && command=="pack") {
                const auto& text=value(); std::uint32_t size=0;
                const auto parsed=std::from_chars(text.data(),text.data()+text.size(),size);
                require(parsed.ec==std::errc{} && parsed.ptr==text.data()+text.size(),ErrorCode::invalid_argument,"Invalid block size");
                options.block_size=size;
            } else {
                require(literal || !arg.starts_with('-'),ErrorCode::invalid_argument,"Unsupported option: " + arg);
                inputs.push_back(native(arg));
            }
        }
        if (command=="pack") {
            require(!output.empty(),ErrorCode::invalid_argument,"pack requires -o");
            ArchiveWriter::pack(inputs,output,options); std::cout << "Archive created and fully verified.\n"; return 0;
        }
        require(inputs.size()==1,ErrorCode::invalid_argument,"Exactly one archive is required");
        ArchiveReader archive(inputs.front());
        if (command=="extract") {
            require(!output.empty(),ErrorCode::invalid_argument,"extract requires -o");
            archive.extract(output); std::cout << "All entries extracted and fully verified.\n";
        } else if (command=="verify") {
            archive.verify(); std::cout << "Full payload verification passed.\n";
        } else if (command=="list") {
            std::cout << "Structurally validated listing (payloads not decoded):\n";
            for (const auto& entry:archive.entries())
                std::cout << (entry.type==EntryType::directory ? "dir\t" : "file\t") << entry.size << '\t' << entry.path << '\n';
        } else {
            std::uint64_t total=0; for (const auto& entry:archive.entries()) total=binary::add(total,entry.size);
            std::cout << "Format: ZYPH 1.0\nIntegrity: SHA-256\nEntries: " << archive.entries().size()
                << "\nOriginal bytes: " << total << "\nArchive bytes: " << archive.archive_size()
                << "\nBlocks: " << archive.block_count() << "\nZstandard blocks: " << archive.zstandard_blocks()
                << "\nSTORE blocks: " << archive.block_count()-archive.zstandard_blocks()
                << "\nValidation: structure and metadata only; use verify for payload integrity.\n";
        }
        return 0;
    } catch (const Error& error) {
        std::cerr << "zyph: " << error.what() << '\n';
        switch (error.code()) {
        case ErrorCode::invalid_argument: return 2;
        case ErrorCode::io: return 3;
        case ErrorCode::corrupt_archive: return 4;
        case ErrorCode::unsupported_feature: return 5;
        case ErrorCode::cancelled: return 7;
        case ErrorCode::source_changed: return 8;
        default: return 6;
        }
    } catch (const std::bad_alloc&) { std::cerr << "zyph: insufficient memory\n"; return 6; }
    catch (const std::exception& error) { std::cerr << "zyph: " << error.what() << '\n'; return 3; }
}
}
#ifdef _WIN32
int wmain(int argc,wchar_t* argv[]) {
    SetConsoleOutputCP(CP_UTF8);
    std::vector<std::string> args;
    for (int i=0;i<argc;++i) {
        const auto size=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,argv[i],-1,nullptr,0,nullptr,nullptr);
        if (!size) { std::cerr << "zyph: invalid Unicode argument\n"; return 2; }
        std::string value(static_cast<std::size_t>(size),'\0');
        if (!WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,argv[i],-1,value.data(),size,nullptr,nullptr)) return 2;
        value.pop_back(); args.push_back(std::move(value));
    }
    return run(args);
}
#else
int main(int argc,char* argv[]) { return run(std::vector<std::string>(argv,argv+argc)); }
#endif
