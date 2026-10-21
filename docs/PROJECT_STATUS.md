# Project status — 2026-10-22

## Milestones

Milestone 0 complete. Milestone 1 complete: format checkpoint `a0feaf7` passed local
Debug/Release (11/11 each) and hosted Windows Debug/Release + Linux ASan/UBSan:
https://github.com/Srijan4O4/Zyphie/actions/runs/39670303400

Milestone 2 engine implemented; local Debug and Release each passed 26/26 tests. Hosted checkpoint
validation is next. Do not mark the milestone complete until these results are recorded.

## Implemented

Exact V1 binary format, checked serialization, maintained SHA-256 providers,
independent golden vectors; scanner, bounded block writing/reading, STORE/Zstandard,
manifest/index, full file verification, transactional new-directory extraction,
second-precision modification times and pack/extract/list/verify/info CLI.
No Qt, account, server, telemetry or custom cryptographic/compression primitive.

## Validation

`./scripts/local-build.ps1 -Preset debug` and `-Preset release` use checksum-verified
LLVM-MinGW Clang 23.1.4, CMake 4.4.4, Ninja 1.13.2 and pinned vcpkg. The suite now has
26 CTest cases: format/overflow/path/hash vectors, independent reader fixtures and
exact writer bytes, mixed-tree STORE/Zstandard round trips, empty/Unicode/spaced
paths, 17 MiB multi-block input, corruption/truncation/forged metadata, resource
limits, symlinks, overwrite refusal, cleanup, pre-cancellation and real CLI workflows.
Final local commands both passed 26/26 tests. A standalone CLI invocation without the compiler PATH also reported version 0.2.0-dev and verified the fixed Zstandard archive. Hosted URLs will be recorded after completion.
No benchmark measurements. No fuzz campaign, hostile filesystem-race proof,
>4 GiB end-to-end test, GUI test or release packaging has been performed.

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
Local tooling is now available under ignored .tools; the previous missing-toolchain
blocker is resolved. Native CLI uses wide Windows arguments and UTF-8 archive paths.

Branch: feature/archive-engine. Milestone 1 commit: a0feaf7. Milestone 2 checkpoint
hash and CI evidence will be recorded after validation. Retain Git history; no
release tag until all release gates genuinely pass.
