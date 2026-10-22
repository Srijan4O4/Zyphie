# Project status — 2026-10-22

## Current milestone

Milestones 0–4 complete. Next: milestone 5, Qt 6 Widgets desktop flows using the
existing Qt-free core. This remains a pre-release CLI/engine, not a V1 release.

## Validated checkpoints

- M1: a0feaf7; 11/11 local Debug/Release plus hosted Windows/Linux.
  https://github.com/Srijan4O4/Zyphie/actions/runs/39670303400
- M2: d5b4204; 27/27 local Debug/Release and hosted Windows/Linux.
  https://github.com/Srijan4O4/Zyphie/actions/runs/39721579235
- M3: 3ff03b639303b607446227016bf13d6aa535d83e, documented in 65c8b3b.
  Local/hosted Windows Debug/Release 39/39; Linux ASan/UBSan 37/37.
  Archive/path fuzz campaigns: 20,000/100,000 iterations, seed 20261022.
  https://github.com/Srijan4O4/Zyphie/actions/runs/39723324891
- M4 source: c51687144b9ac5ac8086f15cf472f1e3b203a201.
  Local LLVM-MinGW Debug/Release and hosted MSVC Debug/Release: 46/46 each.
  Hosted Linux ASan/UBSan: 43/43. Three tests are Windows-specific.
  Actual CLI Ctrl+C/SIGINT tests passed in all three hosted build jobs; exit 7,
  unchanged source, no final or staging output. Archive/path fuzz campaigns
  completed 20,000/100,000 iterations. Final logs contained no compiler warnings,
  sanitizer findings or fuzz crashes.
  https://github.com/Srijan4O4/Zyphie/actions/runs/39787815489

## Implemented

Exact V1 layout, checked serialization, SHA-256, golden archives, STORE/Zstandard,
indexed listing, complete verification and safe staged extraction. M3 adds native
handle-relative operations, private staging permissions, no-replace publication
and adversarial race/fault/corruption/path/resource tests.

M4 adds bounded ordered compression with 1–16 workers, conservative working-memory
admission, caller-thread phase progress, cooperative cancellation and indexed
file/subtree selection. CLI exposes threads, memory, progress and selection.
Selected extraction validates all metadata but does not verify unselected payloads.
Serial and parallel output equality, worker failure propagation, cancellation,
callback failure and selective payload reads have regression coverage.

Windows transient publication contention found during validation is handled with
bounded cancellation-aware retries of sharing/access errors, compatible source
handle sharing and retained no-replace semantics. Child/directory handle contention
has a regression test. An incomplete first benchmark was rejected and rerun fresh.

## Benchmarks and memory gate

Three repetitions, 156 measured process operations; every accepted round trip
verified. Mixed corpus: 289 files, 27,858,143 bytes. Comparisons include retained
M3, STORE, all presets with serial/parallel workers, ZIP, 7z and native Zstandard.
A 4 GiB + 1 MiB source packed at 31.88 MiB peak working set versus 26.89 MiB for
64 MiB with identical block/worker settings. Metadata still grows with block count.
See BENCHMARKS.md and tracked raw/summary results for commands and limitations.

## Reproduction

Local: `./scripts/local-build.ps1 -Preset debug` and `-Preset release`.
Hosted: `cmake --preset <preset>`, `cmake --build --preset <preset>`,
`ctest --preset <preset>` for Windows debug/release and Linux sanitize.
Signal test: `python tests/integration/cancel.py <path-to-zyph>`.
Fuzz commands/budgets: tests/fuzz/README.md and .github/workflows/ci.yml.
Benchmark commands and tool versions: BENCHMARKS.md. Python is test tooling only.
The retained M3 BUILD_TESTING=OFF Release binary built and verified the golden
Zstandard fixture; the measured M4 Release binary includes inactive test hooks.

## Limits and unexecuted validation

See SECURITY.md for confinement. Sources are not snapshots; same-identity or
privileged processes are outside isolation. Existing destinations are refused.
Memory admission is not an RSS cap. No encryption/authentication, ACL/stream/owner
preservation or crash-durability guarantee. Cancellation waits for a codec block.

GUI/release packaging belong to M5/M6 and have not run. Longer fuzz campaigns,
fully instrumented dependencies, AppVerifier/PageHeap and real disk-full/power-loss
experiments remain release assurance work. Benchmarks cover one Windows machine
with uncontrolled warm/mixed cache and generated inputs; cold-cache, real creator
corpora and wider hardware/filesystems remain unmeasured. No universal speed claim.

Validated checkpoints are promoted to main without rewriting history. No release
tag until milestone 6 acceptance passes.
