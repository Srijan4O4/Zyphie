# Changelog

## 0.1.0-dev — unreleased — 2026-10-22

- Removed the previous C/Huffman project from the active tree as requested.
- Preserved the new product requirements and created the engineering blueprint.
- Added a C++20 core/CLI scaffold, CMake presets, pinned vcpkg manifest,
  GoogleTest/Zstandard integration tests and Windows/Linux CI configuration.
- Documented the missing local toolchain, security requirements and milestone gates.
- No archive operations, GUI, performance results or release package are delivered.

## 0.2.0-dev — unreleased — 2026-10-22

- Milestone 1: complete byte-level V1 specification, checked serialization,
  SHA-256 providers, UTF-8/path validation and independent reference archives.
- Milestone 2: regular-file/directory scanner, STORE/Zstandard engine, indexed
  reader, full integrity verification, temporary staging and new-tree extraction.
- Added pack/extract/list/verify/info CLI, Unicode Windows arguments, explicit
  exit codes and documented single-threaded compression presets.
- Added round-trip, corruption, bounds, output-safety and real CLI integration tests.
- Installed verified project-local portable tools; no system compiler replacement.
- Security hardening, selective extraction, parallelism, GUI and release remain pending.
- Fixed MSVC filesystem clock conversion and clarified conditional test assertions for warning-free GCC builds.
- Milestones 1 and 2 validated: final engine suite passed 27/27 locally in Debug/Release and in hosted MSVC Debug/Release plus Linux ASan/UBSan (run 39721579235).

## Milestone 3 — validation in progress

- Resolve native writes and publication relative to checked directory handles.
- Apply private Windows staging DACLs and POSIX modes at creation.
- Add fault, cancellation, mutation, race, collision and permission regressions.
- Add bounded Linux Clang archive/path fuzz campaigns with ASan/UBSan.
