# Third-party software and replacement

Zyphie's source uses MIT (LICENSE). Dependencies retain their own terms.

| Component | Shipped use | License |
| --- | --- | --- |
| Qt 6.8.3 Core, Gui, Widgets and platform/style/image plugins | Dynamic DLLs | LGPL-3.0, with bundled third-party notices |
| Zstandard 1.5.7 | Statically linked archive codec | BSD-3-Clause alternative |
| LLVM-MinGW runtime | Runtime support | Apache-2.0 with LLVM exceptions; MinGW runtime notices |
| Windows system DLLs | OS-provided, not bundled | Supplied by Windows |

GoogleTest and Qt Test are validation-only; their DLLs/executables are excluded
from the user package. Python, aqt, CMake, Ninja and vcpkg are build tools only.

The Windows package includes licenses/, Qt SPDX inventory, and corresponding
QtBase 6.8.3 source under sources/. That source includes the Qt third-party source,
license texts and notices. Qt modules outside QtBase are not used. The inventory
covers the SDK and can list components not shipped by this application.

You may replace the Qt DLLs/plugins with ABI-compatible builds; do not mix versions
within a set. Keep the Qt DLL names and relative plugins directory structure.
Build the included QtBase source using its configure/CMake instructions for shared
Windows x64 LLVM-MinGW libraries (same compatible ABI), then replace the DLLs and
plugins in a COPY of the package. Zyphie imposes no signature check, activation,
DRM or restriction on reverse engineering for debugging library modifications.
The complete MIT application source is also available in the repository and can
be rebuilt against a different supported Qt SDK using DEVELOPMENT.md.

Qt source: https://download.qt.io/archive/qt/6.8/6.8.3/submodules/
Qt terms: https://www.qt.io/development/open-source-lgpl-obligations
Qt licensing: https://doc.qt.io/qt-6.8/licensing.html
Zstandard source: https://github.com/facebook/zstd/tree/v1.5.7
LLVM-MinGW: https://github.com/mstorsjo/llvm-mingw/releases/tag/20261020

The packaging script includes the installed dependency notices and matching source;
do not redistribute an incomplete staging folder or remove those materials.
