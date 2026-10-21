# Project status — 2026-10-22

## Current milestone

**Milestone 0 — Repository Audit and Engineering Blueprint: partially complete.**
The fresh tree and build plan exist. Local build validation is blocked by the
missing C++20 toolchain, CMake, Ninja and vcpkg. No later milestone is complete.

## Completed checkpoints

- Inspected repository root, branches, commits, working tree and remote.
- Confirmed GitHub default branch main and authenticated repository access.
- Replaced the previous implementation with a fresh C++20 project scaffold.
- Preserved the entire supplied product contract in PRODUCT_REQUIREMENTS.md.
- Documented architecture, roadmap, decisions, security and benchmark methodology.
- Added Debug/Release/sanitizer presets, pinned dependencies and CI definition.
- Added toolchain/dependency smoke tests and an honest help/version-only CLI.

## Validation evidence

Audit commands executed: `git status --short`, `git branch --show-current`,
`git log -5 --oneline`, `git remote -v`, `git fetch origin --prune`,
`g++ --version`, `Get-Command cmake,ninja,clang++,g++,cl,vcpkg` and installation
directory checks. GCC reports 6.3.0; other required build tools were not found.
`git ls-remote --tags https://github.com/microsoft/vcpkg.git 'refs/tags/2026*'`
resolved the selected official release to the manifest baseline.

Static checks executed: PowerShell `ConvertFrom-Json` parsed both JSON files;
`git diff --check` passed. A direct capability probe,
`g++ -std=c++20 -fsyntax-only apps/cli/main.cpp -Iinclude`, failed because GCC 6.3
does not recognize `-std=c++20`. This is a confirmed toolchain blocker, not a
source compilation failure under a supported compiler.

Build/test commands required but **not executed** locally:

```powershell
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
cmake --preset release
cmake --build --preset release
ctest --preset release
```

Linux CI additionally must run `cmake --preset sanitize`,
`cmake --build --preset sanitize`, and `ctest --preset sanitize`.
No compiler test result, sanitizer result or hosted CI pass is claimed here.
Benchmarks: **not measured**. UI tests: not applicable; no UI exists.

## Remaining work and blockers

1. Provision the documented local C++20/CMake/Ninja toolchain and pinned vcpkg.
2. Run all foundation builds/tests and inspect CI logs; fix warnings and failures.
3. Finish milestone 0 validation and record its exact commit hash/evidence.
4. Specify the complete binary format and limits at milestone 1. The current
   format document is deliberately only a checklist, not an implementable spec.
5. Then implement STORE round trips before Zstandard archive integration, harden
   extraction before concurrency/GUI, and follow ROADMAP.md through release gates.

## Decisions and limitations

See DECISIONS.md. Core remains Qt-free; dependency versions are baseline-pinned.
Regular files/directories are the planned scope. Archive APIs, format, digest,
filesystem confinement, presets, streaming, cancellation, benchmarks and desktop
are not implemented. Product licensing remains an owner decision before release.

## Git checkpoint

Branch: `main`. Most recent validated new implementation commit: **none**.
Historical starting HEAD: `bbe02dd`; pre-restart main: `5fb0f15`.
The restart checkpoint is identifiable with
`git log -1 --format="%H %s" --grep="chore: initialize Zyphie engineering foundation"`.
Record its hash in a follow-up status entry after creation and actual validation;
do not imply that the foundation has passed runtime tests merely because it is committed.
