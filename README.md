# Zyphie

A native C++20 archive engine, CLI and Qt Widgets desktop with its own documented `.zyph` container.
Windows 10/11 x64 is the first product platform; the core also builds on Linux.

**Status: Zyphie V1 — milestones 0–6 validated.** Desktop, CLI and
portable package validation is recorded in [release evidence](docs/RELEASE_VALIDATION.md).

## Use the desktop

Extract the complete Windows x64 portable ZIP and open `zyphie.exe`. Add files or
folders, choose a new archive path and Create archive. Open an archive to browse,
verify or restore selected entries. Existing destinations are protected.
See the [user guide](docs/USER_GUIDE.md) and [release notes](docs/RELEASE_NOTES.md).
Local builds: `./scripts/desktop-build.ps1`; executable: `build/desktop/apps/desktop/zyphie.exe`.

## Use the CLI

```powershell
./build/release/zyph.exe pack "C:/Data/My folder" -o "C:/Backups/data.zyph"
./build/release/zyph.exe pack file1.txt file2.bin -o files.zyph --store
./build/release/zyph.exe list files.zyph
./build/release/zyph.exe info files.zyph
./build/release/zyph.exe verify files.zyph
./build/release/zyph.exe extract files.zyph -o restored
./build/release/zyph.exe pack "C:/Data/My folder" -o parallel.zyph --threads 4 --progress
./build/release/zyph.exe extract parallel.zyph -o selected --select "My folder/report.txt"
```

Outputs must not already exist; their parent directory must exist. Packing keeps
source basenames, directory structure, empty entries and exact file bytes. Original
inputs are never modified or deleted. Extraction publishes a new directory tree
only after verification succeeds. Archives inside an input directory are rejected.
Use destinations whose parent directories are controlled by you; native handle-relative operations reject links and prevent path-swap redirection.

`--preset fast|balanced|maximum` selects Zstandard levels 1/3/19. `--threads 1..16`
sets compression workers (default 1). At most twice that many blocks are in flight;
output order is deterministic. `--memory-mib` sets the block/codec working-memory
admission budget (default 512 MiB); metadata and process overhead are additional.
`--store` disables compression. Independent blocks fall back to STORE if Zstandard
would not shrink them. `--block-size` accepts 1..16777216 bytes (default 4194304).
Packing fully verifies the temporary archive before publishing its final name.
Listing/info validate structure and metadata without decoding payloads; `verify`
checks all payload blocks and reconstructed file digests. No performance claim
is implied by preset names.

Portable modification times are preserved to whole seconds within the V1 range.
ACLs, permissions, ownership, alternate streams and subsecond timestamps are not
represented. Symlinks, reparse points and special filesystem objects are rejected.
`--select` accepts exact case-sensitive archive paths and can be repeated. A directory
selection includes its descendants; parent directories are retained. Missing paths
fail before output. Only selected file payloads are decoded and verified; use
`verify` to check unselected data too. An empty selection means full extraction.

`--progress` reports phase-local bytes/entries on stderr; scan totals are unknown.
Ctrl+C requests cooperative cancellation and returns exit 7 before publication.
An active codec call finishes its current bounded block before stopping. Byte
completion is not publication success; the final success message confirms that.
Existing outputs remain unsupported. The C++ API exposes matching progress,
selection and stop-token options without Qt dependencies.

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
