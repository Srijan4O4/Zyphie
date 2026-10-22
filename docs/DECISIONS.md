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

## ADR-008 — Filesystem clock portability

The initial engine CI build exposed that MSVC implements file_clock through UTC,
while libc++/libstdc++ expose to_sys/from_sys. Use a shared time helper with MSVC
clock_cast and the respective system-clock conversion elsewhere. This follows
https://learn.microsoft.com/en-us/cpp/standard-library/file-clock-class and keeps
wire timestamps as Unix seconds. Boundary tests include 1901, the Unix epoch,
2023 and 2100. No format change is involved.

## ADR-009 — Native directory-relative operations (2026-10-22)

Path checks alone cannot authorize later writes. Windows now walks one component
at a time through NtCreateFile RootDirectory handles with FILE_OPEN_REPARSE_POINT,
queries each handle and rejects reparse points. Directory handles request list
access and exclude delete sharing. NtSetInformationFile performs a relative,
no-replace rename. Linux uses openat/O_NOFOLLOW/O_DIRECTORY, mkdirat, futimens,
renameat2(RENAME_NOREPLACE) and descriptor-relative cleanup. Windows staging has
a protected owner/SYSTEM DACL; Linux creates mode 0700 directly (no chmod window).
A moved Linux ancestor stays the opened object; a concurrent replacement link
cannot redirect that operation. Same-identity/privileged processes can still
modify owned data and are not an isolation boundary. This is not a snapshot.

A deterministic race regression exposed that Windows FILE_READ_ATTRIBUTES-only
handles did not block directory rename. FILE_LIST_DIRECTORY access plus native
relative operations replaced that insufficient approach. No wire-format change.
References: https://learn.microsoft.com/en-us/windows/win32/api/winternl/nf-winternl-ntcreatefile
and https://man7.org/linux/man-pages/man2/openat.2.html .

## ADR-010 — Reproducible adversarial validation

Internal thread-local test hooks inject read/write/flush/publication/mkdir events
only in BUILD_TESTING builds; no environment or archive field can activate them.
Production BUILD_TESTING=OFF omits the callback. Linux Clang libFuzzer targets test
full archive parsing/verification and UTF-8/path validation independently, under
ASan/UBSan. The archive harness repairs bounded metadata hashes as a second pass
to reach structural checks beneath checksums. Fixed seeds and finite run budgets
make the initial campaign reproducible; absence of findings is not a proof.

Windows cleanup also uses handle-relative enumeration/deletion, including reparse objects. The NT API declaration is provided for MSVC SDKs that omit it; the ntdll import library supplies the existing OS export.

## ADR-011 — Bounded ordered compression and selection

Milestone 4 retains V1 block ordering. One coordinator scans/reads/hashes files,
admits at most 2*threads blocks (one in serial mode), and consumes futures in input
order. Fixed jthread workers own buffers and codec calls only. They never touch
archive handles or callbacks. Worker exceptions travel via packaged_task futures;
shutdown joins active work and discards queued work. Counts are limited to 1..16.
Working-memory admission uses a conservative block-buffer allowance plus pinned
Zstandard's context-size estimator; this excludes metadata and is not an RSS cap.

Selective extraction validates all metadata, binary-searches exact requested
archive paths, includes directory descendants and required ancestors, and decodes
only selected payloads. Missing names fail before staging; duplicate selections
are deduplicated. Unselected payload integrity is explicitly not claimed.
Progress is phase-local and delivered on the caller thread; scan totals are
unknown. Completion of bytes is not publication success. Callback exceptions or
cancellation abort before publication. CLI signal handlers only set a lock-free
atomic flag; a small monitor requests cooperative cancellation outside the handler.
One codec call is not preempted; cancellation is bounded by a block operation.

Windows publication now retries only access-denied/sharing errors up to eight times at 25 ms intervals, retaining pinned parents and no-replace semantics. A real child-handle sharing test reproduces the condition. Each retry checks cancellation; permanent errors still fail explicitly.

Publication-source open permits compatible read/write sharing while excluding
delete sharing; both open and rename retain bounded retries. A fresh complete
benchmark follows the rejected partial run. Measurements show no BALANCED benefit
from four workers on this mixed corpus, so the default remains one. MAXIMUM gains
wall time with four workers at substantially higher CPU/memory cost; expose the
choice rather than select workers implicitly. See BENCHMARKS.md for evidence.
