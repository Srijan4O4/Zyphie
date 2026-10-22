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

## Portable local build used in this workspace

The ignored .tools folder now contains checksum-verified upstream releases:

| Asset | SHA-256 |
| --- | --- |
| mstorsjo/llvm-mingw 20261020 ucrt-x86_64.zip | fd5462b7f5d93b2ff981fcd3f9c7e427e47f3203c64ea91c27bcc444eebd74cd |
| Kitware/CMake v4.4.4 windows-x86_64.zip | bace36e94b31c68ab6fa295f26dfa11219e0701cf7c94b0284a7d1cb13dac536 |
| ninja-build/ninja v1.13.2 ninja-win.zip | 07fc8261b42b20e71d1720b39068c2e14ffcee6396b76fb7a795fb460b78dc65 |

After extracting these official assets under .tools and bootstrapping the pinned
vcpkg checkout, run `./scripts/local-build.ps1 -Preset debug` or `-Preset release`.
The helper sets PATH only for its process and selects x64-mingw-static. Do not mix
MSVC and MinGW in one build directory. Use a fresh build directory when changing
compiler families. Python 3 is used only to regenerate independent fixed fixtures
via `python tests/fixtures/generate.py`; the product has no Python dependency.

## Security validation

The ordinary Debug/Release suites include deterministic native filesystem race,
failure, cancellation and permission tests. Windows additionally checks junctions
and handle counts. Linux `sanitize` runs the portable suite under ASan/UBSan.
See `tests/fuzz/README.md` for the two Linux Clang fuzz targets and exact CI budgets.
Use `-DBUILD_TESTING=OFF` for a production build without the internal fault callback.
No Qt installation is required. Windows links the OS BCrypt, Advapi32 and ntdll
import libraries; no new third-party runtime dependency was introduced.

## Streaming and benchmark validation

Run `python tests/integration/cancel.py <path-to-zyph>` for a real CLI Ctrl+C/SIGINT
check (the helper uses an isolated hidden Windows console). It asserts exit 7,
unchanged source and absence of both published and staged outputs. Hosted CI runs
this after CTest on Windows and Linux. Python is also used by the benchmark runner;
it is not a product runtime dependency. See BENCHMARKS.md for exact reproduction,
baseline versions, raw results and the limitations of timing comparisons.

## Desktop builds

Install aqtinstall 3.3.0 into project-local Python tooling, then obtain the official
SDK with `python -m aqt install-qt windows desktop 6.8.3 win64_llvm_mingw -O .tools/Qt --archives qtbase`.
With the portable LLVM-MinGW toolchain described above, run
`./scripts/desktop-build.ps1` (Release) or `-Configuration Debug -BuildDir build/desktop-debug`.
It configures ZYPH_BUILD_DESKTOP=ON, builds the shared Qt client, runs CTest and
native Windows widget tests. Generated PNGs/test logs stay under the build tree.
For MSVC, use win64_msvc2022_64, the debug/release presets and
`-DZYPH_BUILD_DESKTOP=ON -DCMAKE_PREFIX_PATH=<Qt SDK directory>`. Keep that SDK's
bin directory on PATH while testing. Qt Test is needed only with BUILD_TESTING=ON.
Headless CI uses the offscreen platform; native Windows rendering is checked locally.

## Portable Windows package

Build without hooks using `./scripts/desktop-build.ps1 -NoTests -BuildDir build/desktop-production`.
Obtain corresponding Qt source with
`python -m aqt install-src windows desktop 6.8.3 -O .tools/Qt --archives qtbase`.
Run `./scripts/package-windows.ps1`; it refuses an existing output, includes
runtime DLLs, Qt source, license notices and a SHA-256 manifest. Then run
`python scripts/package-smoke.py <package.zip>` to validate all manifest hashes,
relocate the package, strip SDK paths, run CLI golden/round-trip checks and launch
and close the actual GUI. Use a new -Output path for each candidate.
Qt 6.8.3's deploy tool misclassifies LLVM-MinGW plugins as debug; the script uses
an explicit audited Core/Gui/Widgets, qwindows and Windows style dependency set.
No unrelated Qt modules, test executables or build tools are shipped.
