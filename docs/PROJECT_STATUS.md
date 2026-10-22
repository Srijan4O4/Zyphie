# Project status — 2026-10-22

## Current milestone

Milestones 0–6 validated for Zyphie V1. The product includes a native C++20 engine,
CLI, Qt 6 Widgets desktop and portable Windows x64 package. V2–V4 are future
roadmap work, not part of the completed V1 scope. Source license: MIT, selected
by the owner. Dependencies retain their own notices and licenses.

## Validated checkpoints

- M1 a0feaf7: 11/11 local and hosted tests (run 39670303400).
- M2 d5b4204: 27/27 local and hosted tests (run 39721579235).
- M3 3ff03b6 / documentation 65c8b3b: 39/39 Windows, 37/37 Linux,
  20,000 archive / 100,000 path fuzz iterations (run 39723324891).
- M4 c516871 / documentation 0eb1566: 46/46 Windows, 43/43 Linux,
  fuzz/signal tests, 156 benchmark operations including >4 GiB (run 39787815489).
- M5 desktop c16134d; V1 product/version 44ecb00; final test correction cdb86d7.
  Final hosted run: https://github.com/Srijan4O4/Zyphie/actions/runs/39828853138
  MSVC Debug/Release 47/47 each plus ten extra GUI repetitions each; Linux
  ASan/UBSan 43/43; actual Ctrl+C/SIGINT checks; archive/path fuzz 20,000/100,000.
  No compiler warnings, sanitizer findings or fuzz crashes in final logs.
- M6: clean committed-source build 47/47, production BUILD_TESTING=OFF build,
  relocated source-inclusive package with clean PATH, all golden archives and
  byte-equal round trip, responsive production GUI and normal close. Final
  production benchmark: 144 process operations, three repetitions, all verified.

Full evidence map, failure diagnosis and scope: RELEASE_VALIDATION.md. Package
manifest and SHA256SUMS identify distributed binaries. Binaries and large source
bundles are release assets, not Git-tracked files. Main receives validated state
by fast-forward only; no shared history has been rewritten.

## Delivered behavior

V1 layout/specification, checked serialization, SHA-256, golden archives,
STORE/Zstandard independent blocks, bounded ordered compression, indexed listing,
full verification, selected file/subtree extraction and safe staged publication.
Native handle-relative operations, private staging permissions, no-replace writes,
resource limits and adversarial race/fault/path/corruption coverage remain enforced.

Desktop: create/open/browse/verify/extract, file/folder pickers, drag/drop, light/dark
themes, keyboard access, measured progress, phase estimates and cancellation.
A worker owns each archive job; only the GUI thread touches widgets. Signals and
progress snapshots are bounded. Existing destinations require a new name.

## Reproduction

Core: `./scripts/local-build.ps1 -Preset debug` and `-Preset release`.
Desktop: `./scripts/desktop-build.ps1`; Debug adds `-Configuration Debug -BuildDir build/desktop-debug`.
Hosted: `.github/workflows/ci.yml`; fuzz commands in tests/fuzz/README.md.
Signals: `python tests/integration/cancel.py <path-to-zyph> build/cancel`.
Package: `./scripts/desktop-build.ps1 -NoTests -BuildDir build/desktop-production`,
then `./scripts/package-windows.ps1` and `python scripts/package-smoke.py <zip>`.
Pinned tools, SDK/source setup and clean-build instructions: DEVELOPMENT.md.
User workflows: USER_GUIDE.md. Performance commands/raw data: BENCHMARKS.md.

## Limits and additional assurance

Portable unsigned Windows x64 ZIP; no MSI, shell integration or signing claim.
Local Windows 11 and hosted Windows Server 2022 tested. A separate Windows 10
machine, macOS and Linux desktop binary distribution are not validated here.
Sources are not snapshots. Same-identity/privileged modification is outside
isolation. No existing-output merge/overwrite, encryption, authentication,
ACL/alternate-stream/owner preservation, crash durability or automatic recovery.
Selected extraction does not verify unrelated payloads. Memory admission is not
an RSS cap; metadata scales with bounded entry/block counts. Codec cancellation
waits for its current block.

Long-duration fuzzing, fully instrumented dependencies, AppVerifier/PageHeap and
real disk-full/power-loss experiments remain additional assurance work. Benchmarks
cover one machine with generated inputs and uncontrolled warm/mixed cache, not
universal speed claims. The 4 GiB + 1 MiB workload peaked at 31.88 MiB while packing;
see the report for corpus/method limits. No unexecuted check is claimed as passed.
