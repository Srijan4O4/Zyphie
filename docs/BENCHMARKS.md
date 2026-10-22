# Benchmarks — Milestone 4, 2026-10-22

Source: `c51687144b9ac5ac8086f15cf472f1e3b203a201`. All accepted round trips
passed SHA-256 equality checks. Three repetitions per configuration; 156 measured
process operations. [Raw measurements](../benchmarks/results/2026-10-22-windows.json)
include exact arguments, binary/dataset hashes, CPU time, wall time and peak working
set. [Summary](../benchmarks/results/2026-10-22-summary.json) includes medians/ranges.
Large datasets and binaries remain ignored local files.

## Environment and method

Windows 11 Home Single Language 10.0.26300; Intel Core i5-12500H (12 cores,
16 logical CPUs); 16,831,733,760 bytes RAM; Samsung MZVL2512HCJQ-00BH1 NVMe, NTFS D:.
Clang 23.1.4 Release, CMake 4.4.4, Ninja 1.13.2, pinned Zstandard 1.5.7;
7-Zip 26.03. M3 baseline is the retained production build of 3ff03b6 (65c8b3b
changes documentation only). M4 includes inactive BUILD_TESTING hooks; this
configuration difference prevents attributing every timing change to the pipeline.

Seed 20261022. Mixed corpus: 289 files, 27,858,143 bytes, including 8 MiB random,
16 MiB repeated text, 1 MiB zlib-compressed random, 256 tiny source files, tracked
C++ sources, CSV, JSON, Markdown, TypeScript, generated PNG/PDF and empty entries.
This is synthetic coverage, not a representative professional media corpus.

Cache is uncontrolled warm/mixed; no eviction or cold-cache claim. Wall time
includes process startup. Windows GetProcessTimes measures process CPU time;
GetProcessMemoryInfo samples peak working set every 5 ms and after process exit.
Reported memory is the largest measured peak over three runs, excluding kernel
cache. Background OS activity is uncontrolled. The first incomplete benchmark
failed publication and is excluded; results below are the fresh complete run.

## Mixed corpus

Times are seconds; pack is median [min, max], extraction is median. Peak memory
is pack MiB. Defaults use 4 MiB blocks. CPU times and all other ranges are in JSON.

| Configuration | Archive bytes | Pack seconds [range] | Extract seconds | Peak MiB |
| --- | ---: | ---: | ---: | ---: |
| M3 balanced, 1 worker | 10,369,620 | 0.510 [0.464, 0.510] | 1.507 | 14.95 |
| STORE, 1 | 27,903,290 | 0.484 [0.469, 0.494] | 1.481 | 10.14 |
| FAST, 1 | 10,377,738 | 0.478 [0.446, 0.483] | 1.321 | 14.37 |
| FAST, 4 | 10,377,738 | 0.480 [0.479, 0.481] | 1.271 | 26.47 |
| BALANCED, 1 | 10,369,620 | 0.483 [0.471, 0.553] | 1.271 | 15.11 |
| BALANCED, 4 | 10,369,620 | 0.494 [0.465, 0.503] | 1.293 | 27.07 |
| MAXIMUM, 1 | 10,359,881 | 1.757 [1.701, 1.901] | 1.330 | 62.85 |
| MAXIMUM, 4 | 10,359,881 | 1.435 [1.419, 1.461] | 1.255 | 168.44 |
| ZIP Deflate level 5, 1 | 10,460,336 | 0.548 [0.529, 0.596] | 0.181 | 13.68 |
| 7z LZMA2 level 5, non-solid, 1 | 10,320,208 | 2.525 [2.483, 2.596] | 0.190 | 185.24 |
| Native Zstandard level 3, 1, TAR | 10,325,701 | 0.135 [0.126, 0.361] | 0.038* | 45.49 |

Zyphie pack includes complete read-back verification and SHA-256; baseline pack
commands do not. Native Zstandard consumes a prebuilt TAR: TAR construction and
untarring are excluded (*extraction is decoding TAR bytes only). Other extract
measurements create real trees. Container metadata and safety costs differ.
These are measured workflows, not interchangeable codec-only speed comparisons.

BALANCED/1 verify median: 0.0571 s; list: 0.0167 s; one tiny-file selected extract:
0.0258 s. MAXIMUM/4 reduced median pack wall time about 18% here, while peak memory
rose from 62.85 to 168.44 MiB and median CPU time from 1.734 to 1.906 s. BALANCED/4
showed no advantage here. Keep one worker as default; parallelism is configurable.
No claim that Zyphie is generally faster than existing tools is supported.

## Large-file memory gate

BALANCED, 2 workers, 4 MiB blocks; generated mostly-zero files with a random 1 MiB
tail. Source and extracted hashes match on every repetition. These demonstrate
streaming bounds; they are not incompressible-data throughput measurements.

| Input | Archive bytes | Pack median [range], s | Pack peak MiB | Extract median, s | Extract peak MiB |
| --- | ---: | ---: | ---: | ---: | ---: |
| 64 MiB | 1,052,413 | 0.196 [0.194, 0.196] | 26.89 | 0.142 | 10.89 |
| 4 GiB + 1 MiB | 1,255,789 | 10.436 [10.425, 10.910] | 31.88 | 7.532 | 9.94 |

Input grows approximately 64-fold while peak pack memory grows about 5 MiB.
Content buffering is bounded; metadata grows with block/entry counts and has
separate limits. This evidence satisfies the M4 large-input memory gate without
claiming constant total memory or a hard process RSS ceiling.

## Reproduction

Build Release with `./scripts/local-build.ps1 -Preset release`. Retain a separately
built M3 Release executable for the baseline. Build the Zstandard CLI from the
pinned vcpkg source with ZSTD_BUILD_PROGRAMS=ON; use 7-Zip 26.03. Then:

```powershell
python benchmarks/runner/benchmark.py --zyph build/release/zyph.exe --baseline-zyph build/production/zyph.exe --sevenzip .tools/vcpkg/downloads/tools/7zip-26.03-windows/7z.exe --zstd build/zstd-cli/programs/zstd.exe --repeats 3 --large
```

The runner creates a unique directory under build/benchmarks, checks every round
trip and records results.json. Recorded paths use $REPO/$BENCH_ROOT placeholders.
Allow sufficient disk for source, archive and extraction; originals are never
removed. For an exact corpus, use the recorded source revision. Broader real-world,
cold-cache, network-filesystem and cross-hardware measurements remain future work.

## V1 production rerun

The final production binary (BUILD_TESTING=OFF), source cdb86d7, completed a fresh
three-repetition mixed-corpus run: 144 process measurements and every round trip
verified. The updated tracked-source corpus contains 293 files and 27,880,555
bytes, so do not treat it as byte-identical to the earlier M4 corpus.
[Raw release results](../benchmarks/results/2026-10-22-release-windows.json) and
[summary](../benchmarks/results/2026-10-22-release-summary.json) preserve all settings,
hashes and variation. Same hardware/method and comparison caveats apply.

BALANCED/1: pack median 0.556 s [0.503, 0.570], extract 1.296 s,
archive 10,378,283 bytes, peak pack 15.19 MiB; verify 0.0616 s, list 0.0150 s,
selected extract 0.0318 s. MAXIMUM/1 versus /4: pack 1.983 versus 1.588 s,
peak 62.91 versus 186.80 MiB. Memory variation reinforces that worker admission
is not an RSS cap. The >4 GiB experiment above remains the large-file evidence;
this release rerun did not repeat large files because engine code is unchanged.

Reproduce with the same command above, replacing --zyph with
build/desktop-production/zyph.exe and omitting --large. All baseline commands
remain identical. These measurements support workflow functionality and bounded
streaming, not a general speed superiority claim.
