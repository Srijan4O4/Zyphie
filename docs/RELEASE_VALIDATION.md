# V1 release validation

## Validated V1 gates — 2026-10-22

Milestones 0–6 have passed the documented V1 acceptance gates. Final code/test
checkpoint: cdb86d7af02a60772bb7401d9d79562768503681.
Hosted run: https://github.com/Srijan4O4/Zyphie/actions/runs/39828853138

- MSVC Debug and Release: 47/47 CTest each, then ten additional successful GUI
  workflow repetitions each (11 GUI passes per build including the main suite).
- Linux ASan/UBSan: 43/43. Real CLI Ctrl+C/SIGINT checks passed in all build jobs.
- Fuzz: 20,000 archive and 100,000 path iterations, seed 20261022; no crashes.
- Final job logs contain no compiler warnings or sanitizer findings.
- Local LLVM-MinGW Debug/Release: 47/47, native Windows widget flows and inspected
  light/dark screenshots. Ten local repetitions of the final GUI test passed.
- Clean export of 44ecb00 built and passed 47/47; subsequent changes affect tests,
  packaging and documentation, not product code. Production build omits test hooks.
- Source-inclusive candidate package: 62 manifest hashes verified, relocated with
  SDK paths removed; golden archives, exact round trip, actual GUI responsiveness
  and normal close passed. Final distribution includes additional release docs;
  its per-file manifest and external SHA256SUMS accompany the release asset.
- Final production benchmark: 144 measurements, three repetitions, all round trips
  verified; raw data tracked. The unchanged engine retains M4 >4 GiB memory evidence.

The initial hosted Debug GUI failure was reproduced by repetition and identified
as a test requiring a timer within 20 ms. It now observes real event-loop dispatch
with a bounded timeout. The final hosted repetitions above validate that correction.
No failing test was removed or relabeled as passed.

## Acceptance evidence map

| Contract | Evidence |
| --- | --- |
| C++20 Qt-free core; native CLI | Windows/Linux CI and core-only presets |
| Regular files/directories, empty/Unicode entries, exact bytes, timestamps | Archive/Codecs/Timestamp tests and CLI integration |
| Streaming >4 GiB and bounded workers | M4 raw benchmark report; 31.88 MiB peak for 4 GiB + 1 MiB |
| Listing and selective extraction | Indexed reader tests; unrelated corrupt payload stays unread |
| Corruption/features/path confinement | Format/Security/Paths suites and bounded fuzz campaigns |
| No silent unsupported-object loss/overwrite | Scanner rejection, no-replace and conflict tests |
| Cancellation and original preservation | API fault/cancel tests, actual Ctrl+C/SIGINT, widget cancel test |
| Shared GUI engine and responsiveness | Qt widget workflows, heartbeat and bidirectional CLI verification |
| Clean build and packaging | Clean export of 44ecb00: 47/47 CTest; source-inclusive candidate ZIP relocated, 62 file hashes checked, CLI golden/round-trip and production GUI startup/normal close passed with SDK paths removed |
| Licensing | MIT selected by owner; dynamic Qt with corresponding source/notices; Zstd BSD |
| Benchmarks/specification/checkpoints | Tracked report/raw JSON, specs/ZYPH_FORMAT_V1.md, Git history |

## Scope and known limitations

The V1 deliverable is a Windows x64 portable application ZIP, not an MSI or Store
package. It needs no installation privileges and is unsigned. Windows 11 is the
local GUI/package validation OS; hosted Windows Server 2022 validates MSVC.
A separate Windows 10 machine and macOS have not been tested. Linux core tests
run in CI; the Linux desktop is not a supported binary release in this checkpoint.

Existing outputs are refused rather than replaced or merged. No encryption,
authentication, ACL/alternate-stream preservation, filesystem snapshots, crash
recovery or shell integration. Names and supported objects follow SECURITY.md.
No real disk-full/power-loss experiment, AppVerifier/PageHeap run, fully instrumented
third-party dependencies or long-duration fuzz campaign has been performed. These
are additional assurance work; bounded tests are not a security proof.

Benchmark results describe one machine and generated corpora with uncontrolled
warm/mixed cache. No universal compression or speed claim. The engine implementation
is unchanged from the measured M4 source except its version string; desktop work
does not alter V1 byte layouts or codecs.
