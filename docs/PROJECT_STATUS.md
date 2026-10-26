# Project status — 2026-10-26

## Current milestone

Milestones 0–6 validated for Zyphie V1. The product includes a native C++20 engine,
CLI, Qt 6 Widgets desktop and portable Windows x64 package. V2–V4 are future
roadmap work, not part of the completed V1 scope. Source license: MIT, selected
by the owner. Dependencies retain their own notices and licenses.

## Post-V1 correction — explicit extraction scope

Branch: `fix/explicit-extraction-scope`, based on validated V1 commit `0cd44ec`.
Validated implementation checkpoint: `52c40aa`.
The desktop now has separate Extract all and Extract selected buttons. Extract all
ignores highlighted rows; selected extraction requires a nonempty selection and
shows its row count. Core, CLI, archive format and no-overwrite rules are unchanged.

Evidence on Windows 11, 2026-10-26:

- Before the fix, the new GUI regression failed on the missing selected-action
  control; the other 46 CTest entries passed.
- `./scripts/desktop-build.ps1`: Release build, 47/47 CTest entries passed,
  including the expanded GUI regression. Native widget run: four workflows plus
  init/cleanup passed (6 QtTest results). Light/dark screenshots generated; light
  layout inspected. Logs: `build/desktop/Testing/Temporary/LastTest.log` and
  `build/desktop/apps/desktop/native-tests.txt`.
- Regression covers full extraction despite a selected subtree, subtree and
  multiple selections, empty files/directories, no-selection rejection (including
  direct dispatch), re-enabling controls and clearing selection on archive change.
- `./scripts/desktop-build.ps1 -NoTests -BuildDir build/desktop-production` passed.
- Production CLI verified the user's original archive, then freshly extracted all
  181 files and 39 directories. All paths and file SHA-256 digests matched the
  current originals, which remained unchanged. Local report:
  `build/extraction-check-20261026-em15hn09/report.json`.
- Existing partial output contained 48 matching files in one subtree; the existing
  complete output contained all 181 matching files. No user output was overwritten.

Build and native filesystem checks ran outside the restricted execution sandbox:
the sandbox prevented tool discovery and output publication (Windows error 6);
the same production extraction succeeded outside it. The headless Qt run emitted
offscreen font/size-hint warnings; the native GUI run had no warnings or failures.
The optional Vulkan headers were absent; this Widgets app does not use Vulkan.
Debug, hosted MSVC/Linux sanitizer and fuzz runs were not repeated for this UI-only
change. No new benchmark or release tag is claimed.

Portable correction build (V1 version metadata retained, no new published release):
`build/packages/Zyphie-1.0.0-extraction-fix-windows-x64.zip`, manifest revision
`52c40aa`. SHA-256:
`0f67a49e80cb570a8afbec14a4f1f904cc6cd7df1ca983fede710c362d23a017`.
`python scripts/package-smoke.py build/packages/Zyphie-1.0.0-extraction-fix-windows-x64.zip`
passed: 64 manifest file hashes, relocated clean-PATH execution, all golden
archives, byte-equal round trip, responsive native GUI and normal exit. Report:
`build/package-smoke/installed-0lu6r491/report.json`.

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
