# Development

## Clean Windows setup

Install Microsoft's Visual Studio 2022 Build Tools with Desktop development with
C++, the MSVC x64 toolchain, Windows SDK and C++ CMake tools. Use official Microsoft
installers; no automatic system installation is bundled here. Ensure CMake 3.25+
and Ninja are available. Open an **x64 Native Tools Command Prompt** and run
PowerShell from it to inherit the compiler environment. Confirm:

```powershell
cl
cmake --version
ninja --version
git --version
```

From the repository root, create the pinned vcpkg checkout (first setup only):

```powershell
git clone https://github.com/microsoft/vcpkg.git .tools/vcpkg
git -C .tools/vcpkg checkout 9eec002df1cd109f8f104cd966a763daf9751489
./.tools/vcpkg/bootstrap-vcpkg.bat -disableMetrics
$env:VCPKG_ROOT = (Resolve-Path ./.tools/vcpkg).Path
$env:VCPKG_DISABLE_METRICS = '1'
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
cmake --preset release
cmake --build --preset release
ctest --preset release
```

Stop if any setup command fails. vcpkg's official bootstrap downloads its package
manager executable; configure builds the pinned dependencies. Internet access
is required on a cold dependency cache. No dependency downloads occur in the
product runtime. The VCPKG_ROOT variable must be set again in a fresh shell.

## Linux development and sanitizers

Install GCC 12+ or Clang 16+, CMake 3.25+, Ninja, Git, curl, zip/unzip, tar and
the compiler's sanitizer runtimes using the distribution's package manager.
Use the same vcpkg clone and checkout above, then:

```sh
./.tools/vcpkg/bootstrap-vcpkg.sh -disableMetrics
export VCPKG_ROOT="$PWD/.tools/vcpkg"
export VCPKG_DISABLE_METRICS=1
cmake --preset sanitize
cmake --build --preset sanitize
ctest --preset sanitize
```

The sanitizer option currently instruments project targets, not vcpkg-built
third-party dependencies. A separate fully instrumented dependency configuration
is a milestone 3 task. Desktop builds deliberately fail with an explanatory
message until milestone 5 introduces the real Qt target.

## Tests and evidence

Foundation tests check required C++20 library functionality and the linked
Zstandard library's binary-byte round trip, plus CLI help/version and rejection
of unimplemented commands. These are not archive-format round-trip tests.

CI runs Windows Debug/Release and Linux ASan/UBSan. A configured workflow is not
a passing workflow; record actual run URLs and conclusions in PROJECT_STATUS.md.
Later suites cover format vectors, filesystem round trips, malicious archives,
selective extraction, cancellation, regression fixtures and fuzzing.

## Git workflow

Use `main` for validated checkpoints, `feature/<name>` for substantial changes
and `fix/<name>` for repairs. A develop branch is unnecessary now. Inspect the
diff, build, test, review, update documentation, stage intended files and commit
with a conventional message. Push only when authorized. Do not commit tools,
build outputs, credentials, private settings or benchmark corpora. Do not
force-push or rewrite shared history.

## Upstream references

- [CMake presets](https://cmake.org/cmake/help/v3.29/manual/cmake-presets.7.html)
- [vcpkg source and bootstrap](https://github.com/microsoft/vcpkg/tree/9eec002df1cd109f8f104cd966a763daf9751489)
- [GoogleTest CMake guide](https://github.com/google/googletest/blob/main/docs/quickstart-cmake.md)
- [Zstandard source](https://github.com/facebook/zstd)
