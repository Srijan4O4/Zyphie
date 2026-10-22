#pragma once
#include <chrono>
#include <filesystem>

namespace zyphie {
// MSVC exposes file_clock through UTC, whereas libstdc++/libc++ expose to_sys/from_sys.
// clock_cast follows MSVC's specified UTC/system conversion, including its leap-second policy.
inline auto to_system_time(std::filesystem::file_time_type value) {
#ifdef _MSC_VER
    return std::chrono::clock_cast<std::chrono::system_clock>(value);
#else
    return std::filesystem::file_time_type::clock::to_sys(value);
#endif
}
inline std::filesystem::file_time_type to_file_time(std::chrono::sys_seconds value) {
#ifdef _MSC_VER
    return std::chrono::time_point_cast<std::filesystem::file_time_type::duration>(
        std::chrono::clock_cast<std::filesystem::file_time_type::clock>(value));
#else
    return std::chrono::time_point_cast<std::filesystem::file_time_type::duration>(
        std::filesystem::file_time_type::clock::from_sys(value));
#endif
}
}
