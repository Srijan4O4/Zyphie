#include "zyphie/archive.hpp"
#include "zyphie/version.hpp"
#include <charconv>
#include <atomic>
#include <csignal>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <iostream>
#include <string_view>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif

namespace {
std::atomic<bool> interrupted{false};
static_assert(std::atomic<bool>::is_always_lock_free);
#ifdef _WIN32
BOOL WINAPI control_handler(DWORD event) {
    if(event!=CTRL_C_EVENT && event!=CTRL_BREAK_EVENT)return FALSE;
    interrupted.store(true,std::memory_order_relaxed);return TRUE;
}
#else
void signal_handler(int) { interrupted.store(true,std::memory_order_relaxed); }
#endif
struct Operation {
    std::stop_source stop;
    std::jthread watcher;
#ifndef _WIN32
    using Handler=void(*)(int);
    Handler previous=SIG_DFL;
#endif
    Operation() {
        interrupted.store(false,std::memory_order_relaxed);
#ifdef _WIN32
        zyphie::require(SetConsoleCtrlHandler(control_handler,TRUE),zyphie::ErrorCode::io,"Cannot install cancellation handler");
#else
        previous=std::signal(SIGINT,signal_handler);
#endif
        watcher=std::jthread([this](std::stop_token token) {
            std::mutex mutex;std::condition_variable_any wake;std::unique_lock lock(mutex);
            while(!token.stop_requested()) {
                if(interrupted.load(std::memory_order_relaxed)){stop.request_stop();return;}
                wake.wait_for(lock,token,std::chrono::milliseconds(20),[]{return false;});
            }
        });
    }
    ~Operation() {
        watcher.request_stop();watcher.join();
#ifdef _WIN32
        SetConsoleCtrlHandler(control_handler,FALSE);
#else
        std::signal(SIGINT,previous);
#endif
    }
};
const char* phase_name(zyphie::Phase phase) {
    switch(phase){case zyphie::Phase::scan:return "scan";case zyphie::Phase::pack:return "pack";
        case zyphie::Phase::verify:return "verify";case zyphie::Phase::extract:return "extract";}
    return "unknown";
}

std::filesystem::path native(std::string_view text) {
    return std::filesystem::path(std::u8string(reinterpret_cast<const char8_t*>(text.data()),text.size()));
}
void usage() {
    std::cout << "Zyphie " << zyphie::version() << "\n"
        "Usage:\n"
        "  zyph pack <source>... -o <archive.zyph> [--store] [--preset fast|balanced|maximum] [--block-size bytes] [--threads 1..16] [--memory-mib MiB] [--progress]\n"
        "  zyph extract <archive.zyph> -o <new-directory> [--select archive/path]... [--progress]\n"
        "  zyph list <archive.zyph>\n"
        "  zyph verify <archive.zyph> [--progress]\n"
        "  zyph info <archive.zyph>\n"
        "  zyph --help | --version\n"
        "Destinations must not exist. Inputs are preserved. Use -- before source names beginning with -.\n"
        "Ctrl+C cancels before publication. Overwrite and GUI are not implemented.\n";
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
        ExtractOptions extraction; bool show_progress=false;
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
            } else if (!literal && arg=="--progress" && (command=="pack" || command=="extract" || command=="verify")) { show_progress=true; }
            else if (!literal && arg=="--select" && command=="extract") { extraction.paths.push_back(value()); }
            else if (!literal && (arg=="--threads" || arg=="--memory-mib") && command=="pack") {
                const bool threads=arg=="--threads"; const auto& text=value(); std::uint64_t amount=0;
                const auto parsed=std::from_chars(text.data(),text.data()+text.size(),amount);
                require(parsed.ec==std::errc{} && parsed.ptr==text.data()+text.size() && amount>0,ErrorCode::invalid_argument,"Invalid numeric option");
                if(threads){require(amount<=16,ErrorCode::invalid_argument,"Thread count must be 1..16");options.threads=static_cast<unsigned>(amount);}
                else options.working_memory_bytes=binary::multiply(amount,1048576);
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
        Operation operation; options.cancellation=operation.stop.get_token(); extraction.cancellation=options.cancellation;
        auto last=std::chrono::steady_clock::time_point{}; Phase last_phase=Phase::scan;
        ProgressCallback reporter=[&](const Progress& p) {
            if(!show_progress)return;
            const auto now=std::chrono::steady_clock::now();
            if(p.phase!=last_phase || now-last>=std::chrono::milliseconds(100) || (p.entries_total && p.entries_done==p.entries_total)) {
                if(p.phase==Phase::scan)std::cerr<<"scan: "<<p.entries_done<<" entries discovered\n";
                else std::cerr<<phase_name(p.phase)<<": "<<p.bytes_done<<"/"<<p.bytes_total<<" bytes, "<<p.entries_done<<"/"<<p.entries_total<<" entries\n";
                last=now;last_phase=p.phase;
            }
        };
        options.progress=reporter;extraction.progress=reporter;
        if (command=="pack") {
            require(!output.empty(),ErrorCode::invalid_argument,"pack requires -o");
            ArchiveWriter::pack(inputs,output,options); std::cout << "Archive created and fully verified.\n"; return 0;
        }
        require(inputs.size()==1,ErrorCode::invalid_argument,"Exactly one archive is required");
        ArchiveReader archive(inputs.front());
        if (command=="extract") {
            require(!output.empty(),ErrorCode::invalid_argument,"extract requires -o");
            archive.extract(output,extraction); std::cout << (extraction.paths.empty() ? "All entries extracted and fully verified.\n" : "Selected entries extracted and verified; unselected payloads were not verified.\n");
        } else if (command=="verify") {
            archive.verify(operation.stop.get_token(),reporter); std::cout << "Full payload verification passed.\n";
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
