# Project status — 2026-10-22

## Milestones

Milestones 0, 1 and 2 complete. Next: milestone 3 security and reliability.
This is a pre-release engine, not a V1 release or completed desktop application.

Milestone 1 source checkpoint `a0feaf7` passed local Debug/Release (11/11 each)
and hosted Windows Debug/Release + Linux ASan/UBSan:
https://github.com/Srijan4O4/Zyphie/actions/runs/39670303400

Milestone 2 validated source checkpoint: `d5b4204285ab25115f4cd593ccfeb34d44fdb74d`.
Hosted Windows MSVC Debug, Windows MSVC Release and Linux ASan/UBSan each passed
27/27 tests, with no compiler warnings found in the final job logs:
https://github.com/Srijan4O4/Zyphie/actions/runs/39721579235

## Implemented

Exact V1 binary format, checked serialization, maintained SHA-256 providers,
independent golden vectors; scanner, bounded block writing/reading, STORE/Zstandard,
manifest/index, full file verification, transactional new-directory extraction,
second-precision modification times and pack/extract/list/verify/info CLI.
No Qt, account, server, telemetry or custom cryptographic/compression primitive.

The milestone 2 gate is supported by byte-for-byte mixed-tree round trips with
both codecs, independent golden archives, exact STORE writer bytes and a test
showing indexed listing succeeds without decoding a deliberately corrupt payload
while verification and extraction reject it.

## Validation

Local commands: `./scripts/local-build.ps1 -Preset debug` and
`./scripts/local-build.ps1 -Preset release`; both passed 27/27 tests on the final
source checkpoint. Tools: checksum-verified LLVM-MinGW Clang 23.1.4, CMake 4.4.4,
Ninja 1.13.2 and pinned vcpkg under ignored `.tools`.

Hosted commands: `cmake --preset <preset>`, `cmake --build --preset <preset>` and
`ctest --preset <preset>`, using `debug`/`release` on Windows and `sanitize` on Linux.

The 27 CTest cases cover format/overflow/path/hash vectors, independent reader
fixtures and exact writer bytes, mixed-tree STORE/Zstandard round trips,
empty/Unicode/spaced paths, 17 MiB multi-block input, corruption/truncation/forged
metadata, resource limits, symlinks, overwrite refusal, cleanup, pre-cancellation,
filesystem-clock boundaries and actual CLI workflows. A standalone local CLI
without the compiler PATH reported 0.2.0-dev and verified the fixed Zstandard archive.

The initial milestone 2 run (39672445555) passed Linux sanitizers but exposed an
MSVC filesystem-clock API mismatch. The shared conversion helper fixed it;
63c9af3 passed all three hosted jobs (39721176288). The final checkpoint also
removed GCC warnings around conditional GoogleTest assertions.

No benchmarks, fuzz campaign, hostile filesystem-race proof, >4 GiB end-to-end test,
GUI test or release packaging has been performed; these belong to later gates.
Linux sanitizers instrument project targets, not all dependency builds.

## Remaining after milestone 2

Milestone 3: filesystem race confinement, fuzzing, fault injection and adversarial
Windows diagnostics. Milestone 4: bounded parallelism, selective extraction,
progress/CLI cancellation, measured large-file memory and benchmarks. Milestone 5:
Qt Widgets. Milestone 6: packaging, licensing and release acceptance.

## Known limitations

Existing extraction destinations and overwrite are deliberately unsupported.
Destination/source parents must be trusted against hostile concurrent changes;
see SECURITY.md. Metadata limits are not total process-memory limits. Timestamps
are whole seconds within the documented V1 range. No snapshot, encryption,
authentication, ACL/stream/permission preservation or performance claim.
Local tooling is available under ignored .tools; the missing-toolchain blocker
is resolved. Native CLI uses wide Windows arguments and UTF-8 archive paths.

Validated source is promoted to main with this documentation checkpoint. Git
history is retained. Do not create a release tag until all release gates pass.

## Milestone 3 checkpoint — validation in progress

Native directory-relative no-follow I/O, private staging ACLs/modes, relative
no-replace publication and Linux descriptor-relative cleanup are implemented.
Nine additional tests cover deterministic failure injection, in-flight
cancellation, source mutation, publication races, ancestor swaps, symlink cleanup,
private permissions and native case collisions. Debug passed 36/36 locally.
Release and hosted MSVC/Linux sanitizer validation, plus the newly configured
20,000 archive and 100,000 path libFuzzer iterations, remain pending. Do not mark
milestone 3 complete before the evidence is recorded.
