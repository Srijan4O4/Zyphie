# Project status — 2026-10-22

## Current milestone

Milestones 0–3 complete. Next: milestone 4 streaming/concurrency, selective
extraction, progress/cancellation, large-file memory measurements and benchmarks.
This is a pre-release CLI/engine; Qt and release packaging remain later gates.

## Validated checkpoints

- M1: a0feaf7; 11/11 local Debug/Release plus hosted Windows/Linux.
  https://github.com/Srijan4O4/Zyphie/actions/runs/39670303400
- M2: d5b4204; 27/27 in both local builds and all three hosted jobs.
  https://github.com/Srijan4O4/Zyphie/actions/runs/39721579235
- M3 source: 3ff03b639303b607446227016bf13d6aa535d83e; local Debug/Release
  39/39 each, hosted MSVC Debug/Release 39/39 each, Linux ASan/UBSan 37/37.
  Two tests are Windows-specific. No compiler warnings or sanitizer findings
  appeared in the final job logs. Both fuzz targets completed without crashes:
  20,000 archive iterations and 100,000 path iterations, seed 20261022.
  https://github.com/Srijan4O4/Zyphie/actions/runs/39723324891

## Implemented

Exact V1 format, checked serialization, maintained SHA-256 providers, independent
golden archives; regular-file/directory scanner, bounded STORE/Zstandard blocks,
indexed reader, verification, temporary staging and full-tree extraction. CLI:
pack/extract/list/verify/info, Unicode Windows arguments, explicit error codes.

M3 adds native handle-relative no-follow operations on Windows/Linux, private
staging DACLs/modes at creation, no-replace publication and handle-relative cleanup.
Tests cover malformed metadata, truncation/corruption, hostile paths, resource
limits, overwrite refusal, source mutation, injected failures, in-flight cancel,
ancestor/junction races, cleanup, case collisions, permissions and handle leaks.
The race regression caught an insufficient Windows metadata-only locking approach;
it was replaced. MSVC's omitted NT API declaration was corrected before the final run.

One earlier local CLI run encountered Windows access-denied during publication
and reported failure; subsequent complete runs passed. Native I/O errors remain
explicit failures. No silent retries, skipped entries or output overwrites.

## Reproduction

Local: `./scripts/local-build.ps1 -Preset debug` and `-Preset release`, using
verified portable LLVM-MinGW 23.1.4/CMake 4.4.4/Ninja 1.13.2 and pinned vcpkg.
Hosted: `cmake --preset <preset>`, `cmake --build --preset <preset>`,
`ctest --preset <preset>` for Windows debug/release and Linux sanitize.
Fuzz commands and budgets: tests/fuzz/README.md and .github/workflows/ci.yml.
A separate local Release build with BUILD_TESTING=OFF built successfully and
`build/production/zyph.exe verify tests/fixtures/zstd-rle.zyph` passed, confirming
the production configuration without fault hooks.

## Limits and unexecuted validation

See SECURITY.md for the precise confinement model. Same-identity/privileged
processes are outside the isolation boundary; sources are not snapshots. Existing
destinations remain unsupported. Metadata limits are not RSS limits. No encryption,
authentication, ACL/stream/ownership preservation or crash-durability guarantee.

No benchmark, >4 GiB end-to-end/memory measurement, GUI test, release package,
long-running fuzz campaign, fully sanitizer-instrumented dependency build,
Application Verifier/PageHeap or real disk-full/power-loss experiment has run.
Fault injection and bounded project-level sanitizer campaigns do not replace those
additional release checks. No performance or release-security claim is made.

Validated milestones are promoted to main without rewriting history. No release
tag until milestone 6 acceptance is genuinely satisfied.
