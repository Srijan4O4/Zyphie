# V1 release validation

## Candidate status

Version 1.0.0 is being validated; no release tag is authorized by test evidence yet.
M0–M4 passed their documented gates. M5 local Release/Debug CTest 47/47 and native
Qt workflows passed; first hosted Release and Linux sanitizer jobs passed, but
hosted Debug GUI validation failed without diagnostics. A log-capturing runner
has been added to diagnose the failure before release.

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
| Clean build and packaging | Pending final candidate clean-build and relocated-package checks |
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
