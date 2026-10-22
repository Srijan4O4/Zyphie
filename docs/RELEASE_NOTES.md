# Zyphie 1.0.0

Native Windows archive creation, browsing, verification and restoration through a
Qt 6 Widgets desktop and a C++20 CLI. Both clients use the same independently
specified .zyph engine. This release introduces the complete V1 workflow; ZIP,
RAR and 7z import are not supported.

- STORE and Zstandard Fast/Balanced/Maximum modes, bounded parallel workers.
- Indexed archive browsing and selected file/folder extraction.
- Whole-file and block SHA-256 verification before successful publication.
- Protected staging, hostile-path checks and no-replace output semantics.
- Measured progress, safe cancellation, drag/drop and light/dark desktop themes.
- Portable Windows x64 ZIP with runtime dependencies, matching Qt source and notices.

Extract the complete ZIP into a new folder and launch zyphie.exe. The CLI is
zyph.exe. See docs/USER_GUIDE.md in the package. No administrator access or account
is required. The package is unsigned and does not register file associations.

Known limitations: existing destinations are refused; regular files/directories
and whole-second timestamps only. No encryption, authentication, ACL/alternate
stream preservation, filesystem snapshot, crash recovery or third-party format
import. Windows 11 GUI/package and Windows Server 2022 builds were exercised;
Windows 10 remains the target but has not been tested on a separate machine.

Validation and benchmark details are in docs/RELEASE_VALIDATION.md and
BENCHMARKS.md. Measurements describe one machine and generated data, not a general
performance advantage. A 4 GiB + 1 MiB input used 31.88 MiB peak pack working set
in the documented M4 workload. Source code uses MIT; dependencies retain their
respective terms.
