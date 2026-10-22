# Zyphie

A native C++20 archive engine and CLI with its own documented `.zyph` container.
Windows 10/11 x64 is the first product platform; the core also builds on Linux.

**Status: Milestone 3 pre-release engine.** Packing, indexed listing, full SHA-256
verification and full extraction work for supported regular files/directories.
The Qt desktop application and release packaging
are not complete. See [project status](docs/PROJECT_STATUS.md) for exact evidence.

## Use the CLI

```powershell
./build/release/zyph.exe pack "C:/Data/My folder" -o "C:/Backups/data.zyph"
./build/release/zyph.exe pack file1.txt file2.bin -o files.zyph --store
./build/release/zyph.exe list files.zyph
./build/release/zyph.exe info files.zyph
./build/release/zyph.exe verify files.zyph
./build/release/zyph.exe extract files.zyph -o restored
```

Outputs must not already exist; their parent directory must exist. Packing keeps
source basenames, directory structure, empty entries and exact file bytes. Original
inputs are never modified or deleted. Extraction publishes a new directory tree
only after verification succeeds. Archives inside an input directory are rejected.
Use destinations whose parent directories are controlled by you; native handle-relative operations reject links and prevent path-swap redirection.

`--preset fast|balanced|maximum` selects Zstandard levels 1/3/19 with one worker.
`--store` disables compression. Independent blocks fall back to STORE if Zstandard
would not shrink them. `--block-size` accepts 1..16777216 bytes (default 4194304).
Packing fully verifies the temporary archive before publishing its final name.
Listing/info validate structure and metadata without decoding payloads; `verify`
checks all payload blocks and reconstructed file digests. No performance claim
is implied by preset names.

Portable modification times are preserved to whole seconds within the V1 range.
ACLs, permissions, ownership, alternate streams and subsecond timestamps are not
represented. Symlinks, reparse points and special filesystem objects are rejected.
No overwrite, selected extraction, worker pool, CLI cancellation/progress or GUI
is provided yet. The C++ API has basic cooperative stop-token support.

CLI exit codes: 0 success; 2 usage; 3 I/O; 4 corruption; 5 unsupported feature;
6 resource/path/destination conflict; 7 cancellation; 8 changing source.

## Build and test

See [clean-machine setup](docs/DEVELOPMENT.md). Requires C++20, CMake 3.25+, Ninja,
Git and the pinned vcpkg checkout. Core builds do not require Qt. With VCPKG_ROOT
set in an x64 developer shell:

```powershell
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
```

Release uses the same sequence with `release`; Linux ASan/UBSan uses `sanitize`.
This workspace also has verified portable tools under ignored `.tools`; run
`./scripts/local-build.ps1 -Preset debug` or `-Preset release` to use them.

Dependencies: Zstandard, GoogleTest for testing, SHA-256 via Windows BCrypt or
OpenSSL EVP on non-Windows. All vcpkg dependencies use a committed baseline.

## Engineering documentation

- [Original requirements](docs/PRODUCT_REQUIREMENTS.md)
- [V1 binary specification](specs/ZYPH_FORMAT_V1.md)
- [Architecture](docs/ARCHITECTURE.md), [decisions](docs/DECISIONS.md), [roadmap](docs/ROADMAP.md)
- [Security boundaries](docs/SECURITY.md), [benchmarks](docs/BENCHMARKS.md)
- [Changelog](docs/CHANGELOG.md), [initial repository audit](docs/REPOSITORY_AUDIT.md)

Historical Git commits remain recoverable; the former C/Huffman prototype has
been replaced. No prototype-format compatibility is promised. The project's
release license remains undecided; no distribution-ready release is claimed.
