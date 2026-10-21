# Repository audit — 2026-10-22

## Before the restart

Root: `D:\Zyphie`. Remote: `https://github.com/Srijan4O4/Zyphie.git`.
GitHub default branch: `main`. Starting local branch: `phase-0-toolchain`.
Starting HEAD: `bbe02dd` (Phase 0: repair the build, move engine to engine/, add CI).
Remote/local main: `5fb0f15` (Added Makefile). Git fetch confirmed no new commits.
Earlier commits: `61de9c4` (MinHeap constructors), `b1ec42e` (C engine scaffolding).

Tracked contents: C/Huffman sources, Makefile, old format notes, CI,
`.gitignore`, `.gitattributes`, `CLAUDE.md`, and an empty README. No tracked
modifications were found. `.claude/settings.local.json` was untracked in the
restricted-shell view; it is not included in the new project.

The user explicitly requested removing the old folder/repository contents and
starting fresh with the new contract. The current tree was replaced on main.
The `.git` directory, existing remote URL, historical commits and historical
phase-0-toolchain branch were retained. No shared history was force-rewritten.
A recovery copy of the previous main working-tree files and local settings was
saved outside the project in the task artifact directory. The starting branch's
tracked implementation remains recoverable at `bbe02dd`.

## Tools actually inspected

- `git`, GitHub CLI: available; existing GitHub authentication works.
- `g++ --version`: MinGW.org GCC 6.3.0, unsuitable for this C++20 contract.
- CMake, CTest, Ninja, vcpkg, MSVC `cl`, and Clang: not found in PATH.
- No Visual Studio installation found in standard locations or through vswhere.
- No alternative compiler found in the inspected JetBrains installation path.
- Windows Package Manager is present outside the restricted shell.
- No existing C++ dependencies or Qt installation identified in the project.

The restricted shell initially could not access GitHub or the user's global Git
ignore file. A permitted unrestricted Git fetch and authentication check worked.
This is distinct from the missing toolchain.

## Build plan

Install an official Visual Studio 2022 C++ Build Tools toolchain including the
Windows SDK and CMake/Ninja tools; use an x64 native developer shell. Pin vcpkg
to `9eec002df1cd109f8f104cd966a763daf9751489` (2026.10.16, resolved from the official
repository tag). Build Debug and Release locally. Use Linux GCC/Clang CI for
ASan/UBSan. See DEVELOPMENT.md for commands. No system tools were installed
during this repository replacement.
