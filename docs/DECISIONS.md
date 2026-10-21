# Engineering decisions

## ADR-001 — Fresh current tree, recoverable history (2026-10-22)

Replace the C prototype with the supplied C++20 engineering contract. Retain the
existing repository and remote, historical commits and historical branch. The
user authorized replacement locally and on GitHub; repository deletion or a
history rewrite is unnecessary to achieve a fresh implementation tree.

## ADR-002 — Native core first

Use C++20 and CMake 3.25+ with Ninja presets. Qt 6 Widgets is deferred to milestone
5 and never linked into the core. Existing GCC 6.3 is not a viable toolchain.
The Windows reference toolchain is VS 2022 C++ Build Tools plus Windows SDK;
Linux CI provides a second compiler/platform and sanitizer coverage.

## ADR-003 — Reproducible dependency graph

Use vcpkg manifest mode. Pin both checkout and manifest baseline to
`9eec002df1cd109f8f104cd966a763daf9751489` (official tag 2026.10.16).
Initial dependencies: Zstandard for the established codec and GoogleTest for
test discovery under CTest. Add a maintained digest library after its API,
license and portable build have been reviewed at milestone 1. Do not write a
cryptographic hash implementation. Qt dependencies are not installed for core.

Pinning dependencies does not prove their security. Updates require release
note/advisory review and regression tests. Hosted CI images are mutable; this
is a reproducible source/dependency recipe, not a bit-for-bit build claim.

## ADR-004 — Conservative format and extraction scope

Independent blocks, STORE/Zstandard, explicit fields, indexed manifest and
footer, per-block and whole-file integrity. V1 guarantees regular files and
directories only. Explicitly reject unsupported objects by default. Resolve
Windows path aliases and reparse-point races before claiming safe extraction.

## ADR-005 — Gate-based progress

A generated scaffold or successful compilation is not a release. The format
document remains explicitly a design checklist until all binary layouts and
test vectors are agreed and tested. No pack/extract placeholder may report
success. The license for Zyphie itself remains undecided until the owner chooses;
do not add an arbitrary license or publish a release package.

## ADR-006 — V1 wire format and integrity (2026-10-22)

V1 uses 32-byte headers, independent blocks, a typed manifest, a 56-byte-per-block
index and a 128-byte footer. SHA-256 protects decoded blocks, complete files,
metadata and footer fields. Windows uses BCrypt/CNG; non-Windows uses OpenSSL EVP
from the pinned vcpkg manifest. The wire algorithm is identical and independent
of the platform provider. Neither provider is a custom cryptographic primitive.
References: https://learn.microsoft.com/en-us/windows/win32/api/bcrypt/nf-bcrypt-bcryptcreatehash
and https://docs.openssl.org/3.0/man3/EVP_DigestInit/ .
OpenSSL is Apache-2.0 licensed; dependency notices must be included when distributing
its libraries. Windows BCrypt is an OS facility. No authenticity is claimed.

## ADR-007 — Local portable validation toolchain

Retain MSVC CI as the reference Windows build. Add project-local LLVM-MinGW
23.1.4 (release 20261020), CMake 4.4.4 and Ninja 1.13.2 for fast local iteration.
Downloads were taken from upstream release assets and SHA-256 verified before use.
Tools live under ignored .tools; no installed compiler or system PATH was changed.
The vcpkg x64-mingw-static triplet is a community triplet, so hosted MSVC/Linux
checks remain required in addition to local validation.
