# Project status — 2026-10-22

Milestone 0 complete. Milestone 1 implementation and local Debug/Release validation complete;
Both local builds passed 11/11 tests. Hosted validation is pending.

Implemented: exact V1 format specification, checked little-endian primitives,
SHA-256 provider integration, header/footer codecs, strict UTF-8/path validation,
five independent reference archives and GoogleTest/CTest integration.

Local command: `./scripts/local-build.ps1 -Preset debug`: 11/11 tests passed with
LLVM-MinGW Clang 23.1.4. This resolves the former missing-local-toolchain blocker.
The initial test build exposed GoogleTest macro label collisions on shared source
lines; those were fixed and the successful run compiled without project warnings.
No archive engine/GUI or benchmark claims. Spec: specs/ZYPH_FORMAT_V1.md.

Next: complete Release/hosted checks, then implement reader/writer/STORE/Zstandard
round trips and pack/extract/list/verify/info CLI for milestone 2. Milestones 3-6
remain pending. Refer to ROADMAP.md and PRODUCT_REQUIREMENTS.md for mandatory gates.
Git branch: feature/format-v1. Foundation validated source: 659ab1d.
