# Fuzzing

Linux Clang, the pinned vcpkg toolchain and CMake are required. Configure with
`-DZYPH_BUILD_FUZZERS=ON -DZYPH_ENABLE_SANITIZERS=ON -DBUILD_TESTING=OFF`.
The workflow `.github/workflows/ci.yml` contains the full reproducible commands.

`zyph_fuzz_archive` runs the production reader and verifier with limits of 64
entries, 128 blocks, 64 KiB metadata and 1 MiB archive/decoded totals. Each input
is tested raw and, where section offsets are valid, with repaired metadata
checksums to reach deeper parser code. Files live only in a private mkdtemp
workspace. No extraction is performed by this harness; native write safety is
covered by deterministic regression tests.

`zyph_fuzz_path` exercises path validation independently. CI seeds archive fuzzing
with every committed golden archive, uses seed 20261022, and executes 20,000 archive
and 100,000 path iterations with per-input timeouts and RSS limits. Project code
is instrumented; prebuilt dependency internals are not. Longer campaigns and
fully instrumented dependencies remain useful release validation.

Crash artifacts are written under build/fuzz-artifacts. Reproduce with
`zyph_fuzz_archive <artifact>` or `zyph_fuzz_path <artifact>`, minimize with
libFuzzer's `-minimize_crash=1`, and retain a small regression in tests/fixtures.
Do not classify an infrastructure failure as a clean fuzz run.
