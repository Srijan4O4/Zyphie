# Zyphie

A fresh C++20 foundation for a local-first Windows archive application and CLI,
with an independently specified `.zyph` container. Windows 10/11 x64 is the first
product platform; Linux CI checks core portability and sanitizers.

**Status: Milestone 0 engineering foundation. Not an archiver or a release yet.**
The CLI scaffold only supports `--help` and `--version`. Packing, listing,
verification, extraction, and the Qt desktop application are not implemented.
Do not use this project to store irreplaceable data yet.

## Build

Requires a C++20 compiler (Windows: Visual Studio 2022 C++ Build Tools), CMake
3.25 or newer, Ninja, Git, and the pinned vcpkg checkout. Qt is not required.
Follow [the clean-machine setup](docs/DEVELOPMENT.md), then run in an x64
developer shell:

```powershell
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
./build/debug/zyph.exe --help
```

The same configure/build/test sequence supports `release`. The `sanitize` preset
requires GCC or Clang with ASan/UBSan on Linux. Dependencies are Zstandard and
GoogleTest; the vcpkg baseline fixes their port versions.

## Engineering contract

- [Original product requirements](docs/PRODUCT_REQUIREMENTS.md)
- [Project status and evidence](docs/PROJECT_STATUS.md)
- [Repository audit](docs/REPOSITORY_AUDIT.md)
- [Architecture](docs/ARCHITECTURE.md)
- [Milestones and exit gates](docs/ROADMAP.md)
- [Decisions](docs/DECISIONS.md)
- [Format design status](specs/ZYPH_FORMAT_V1.md)
- [Security model](docs/SECURITY.md)
- [Benchmark methodology](docs/BENCHMARKS.md)
- [Changes](docs/CHANGELOG.md)

The former C/Huffman prototype was removed from the current tree on 2026-10-22.
Historical Git commits remain recoverable. No compatibility with that prototype
is promised. No product license or redistribution permission is selected yet;
release licensing is a milestone 6 decision.
