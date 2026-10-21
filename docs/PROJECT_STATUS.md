# Project status — 2026-10-22

## Current milestone

**Milestone 0 — Repository Audit and Engineering Blueprint: complete.**
The fresh tree, build plan and architecture are established. Hosted Windows
Debug/Release and Linux sanitizer builds passed. Local builds remain unavailable
until C++20 tooling is installed. Milestone 1 is next; no later gate is complete.

## Completed checkpoints

- Inspected repository root, branches, commits, working tree and remote.
- Confirmed GitHub default branch main and authenticated repository access.
- Replaced the previous implementation with a fresh C++20 project scaffold.
- Preserved the supplied contract in PRODUCT_REQUIREMENTS.md.
- Documented architecture, roadmap, decisions, security and benchmark methodology.
- Added Debug/Release/sanitizer presets, pinned dependencies and CI.
- Added toolchain/dependency tests and an honest help/version-only CLI.

## Validation evidence

Source checkpoint: `659ab1d66cf7173c2668af62e5ee80d16c0323ce`.
CI run: https://github.com/Srijan4O4/Zyphie/actions/runs/39667567735

| Environment | Commands executed in hosted CI | Result |
| --- | --- | --- |
| Windows Server 2022, MSVC 19.44.35229.0 | `cmake --preset debug`; `cmake --build --preset debug`; `ctest --preset debug` | 5/5 passed |
| Windows Server 2022, MSVC 19.44.35229.0 | `cmake --preset release`; `cmake --build --preset release`; `ctest --preset release` | 5/5 passed |
| Ubuntu 24.04, GCC 13.3.0, ASan/UBSan | `cmake --preset sanitize`; `cmake --build --preset sanitize`; `ctest --preset sanitize` | 5/5 passed |

Tests cover C++20 library facilities, Zstandard binary round trip, CLI help,
version, and rejection of unimplemented pack. They do not test .zyph archives.
Dependencies resolved to GoogleTest 1.18.0 and Zstandard 1.5.7. No project compiler
warnings were found in reviewed logs. The original workflow warned about checkout
v4's deprecated Node runtime; it now pins official checkout v6. C++ source is unchanged.
Third-party dependencies themselves are not sanitizer-instrumented by this preset.
Benchmarks: **not measured**. UI tests: not applicable; no UI exists.

Local checks executed: repository/Git/toolchain audit; JSON parsing with PowerShell
`ConvertFrom-Json`; `git diff --check`; source attachment hash comparison before
Git line-ending normalization. Intentional Markdown hard-break spaces in the
supplied requirements have a path-specific Git whitespace allowance.

Local build/test commands were **not executed**: CMake, Ninja, vcpkg and a modern
compiler are absent. `g++ -std=c++20 -fsyntax-only apps/cli/main.cpp -Iinclude`
failed because installed GCC 6.3 does not recognize `-std=c++20`. This confirms a
toolchain blocker, not a source failure under a supported compiler. No system
compiler installation was performed.

## Remaining work and blockers

1. Install the documented local C++20/CMake/Ninja toolchain and pinned vcpkg,
   then reproduce the passing hosted builds before iterative engine work.
2. Specify the complete binary format and limits at milestone 1. The current
   format document is a checklist, not an implementable specification.
3. Implement foundational serialization and test fixed vectors before the engine.
4. Implement STORE round trips before Zstandard archive integration, harden
   extraction before concurrency/GUI, and follow ROADMAP.md through release gates.

## Decisions and limitations

See DECISIONS.md. Core remains Qt-free and dependencies are baseline-pinned.
Regular files/directories are the planned scope. Archive APIs, format, digest,
filesystem confinement, compression presets, streaming, cancellation, benchmarks
and desktop are not implemented. Product licensing remains an owner decision
before release. No performance or archive security claim is made.

## Git checkpoint

Target branch: `main`, fast-forwarded from `feature/fresh-foundation` after CI.
Most recent validated C++ source checkpoint:
`659ab1d66cf7173c2668af62e5ee80d16c0323ce`.
Follow-up documentation/CI commits record evidence and update the checkout action;
they do not alter the validated C++ source. Use `git log -3 --oneline` for the
latest maintenance hash. Historical starting HEAD: `bbe02dd`; old main: `5fb0f15`.
Existing history is retained; no force-push, repository recreation or release tag.
