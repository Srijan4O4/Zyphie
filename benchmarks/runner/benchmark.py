"""Reproducible synthetic benchmarks. Generates only new, disposable workspaces."""
import argparse
import ctypes
from ctypes import wintypes
import hashlib
import json
import os
from pathlib import Path
import platform
import random
import shutil
import statistics
import subprocess
import tarfile
import tempfile
import time
import zlib

parser = argparse.ArgumentParser()
parser.add_argument("--zyph", type=Path, required=True)
parser.add_argument("--baseline-zyph", type=Path)
parser.add_argument("--sevenzip", type=Path, required=True)
parser.add_argument("--zstd", type=Path, required=True)
parser.add_argument("--work-parent", type=Path, default=Path("build/benchmarks"))
parser.add_argument("--repeats", type=int, default=3)
parser.add_argument("--large", action="store_true")
args = parser.parse_args()
for key in ("zyph", "baseline_zyph", "sevenzip", "zstd"):
    value = getattr(args, key)
    if value: setattr(args, key, value.resolve())
args.work_parent.mkdir(parents=True, exist_ok=True)
root = Path(tempfile.mkdtemp(prefix="run-", dir=args.work_parent.resolve()))
print(f"Workspace: {root}", flush=True)
rows = []
metadata = {"platform": platform.platform(), "logical_cpus": os.cpu_count(), "seed": 20261022,
            "repeats": args.repeats, "cache": "uncontrolled warm/mixed; no cache eviction", "rows": rows}
if os.name == "nt":
    class Counters(ctypes.Structure):
        _fields_ = [("cb", wintypes.DWORD), ("faults", wintypes.DWORD)] + [(name, ctypes.c_size_t) for name in
            ("peak_working_set", "working_set", "peak_paged", "paged", "peak_nonpaged", "nonpaged", "pagefile", "peak_pagefile")]
    psapi = ctypes.WinDLL("psapi", use_last_error=True)
    psapi.GetProcessMemoryInfo.argtypes = [wintypes.HANDLE, ctypes.POINTER(Counters), wintypes.DWORD]
    kernel = ctypes.WinDLL("kernel32", use_last_error=True)
    kernel.GetProcessTimes.argtypes = [wintypes.HANDLE] + [ctypes.POINTER(wintypes.FILETIME)] * 4

def remove_output(path):
    resolved = path.resolve()
    assert resolved != root.resolve() and resolved.is_relative_to(root.resolve())
    shutil.rmtree(resolved)

def digest(path):
    with path.open("rb") as stream: return hashlib.file_digest(stream, "sha256").hexdigest()

def manifest(path):
    files = sorted(p for p in path.rglob("*") if p.is_file()) if path.is_dir() else [path]
    return {str(p.relative_to(path.parent)).replace("\\", "/"): {"size": p.stat().st_size, "sha256": digest(p)} for p in files}

def measure(label, operation, command, cwd, byte_count, repetition):
    index = len(rows)
    started = time.perf_counter()
    with (root / f"{index:03}-stdout.log").open("wb") as out, (root / f"{index:03}-stderr.log").open("wb") as err:
        process = subprocess.Popen([str(x) for x in command], cwd=cwd, stdout=out, stderr=err)
        peak = 0
        while process.poll() is None:
            if os.name == "nt":
                counters = Counters(); counters.cb = ctypes.sizeof(counters)
                if psapi.GetProcessMemoryInfo(int(process._handle), ctypes.byref(counters), counters.cb):
                    peak = max(peak, counters.peak_working_set)
            time.sleep(0.005)
        elapsed = time.perf_counter() - started
        cpu = None
        if os.name == "nt":
            counters = Counters(); counters.cb = ctypes.sizeof(counters)
            if psapi.GetProcessMemoryInfo(int(process._handle), ctypes.byref(counters), counters.cb): peak = max(peak, counters.peak_working_set)
            times = [wintypes.FILETIME() for _ in range(4)]
            if kernel.GetProcessTimes(int(process._handle), *(ctypes.byref(t) for t in times)):
                cpu = sum((t.dwHighDateTime << 32) + t.dwLowDateTime for t in times[2:]) / 1e7
        if process.returncode != 0:
            raise RuntimeError(f"{label}/{operation} failed ({process.returncode}): {(root / f'{index:03}-stderr.log').read_text(errors='replace')}")
    row = {"label": label, "operation": operation, "repetition": repetition, "seconds": elapsed,
           "cpu_seconds": cpu, "peak_working_set_bytes": peak or None, "input_bytes": byte_count,
           "mib_per_second": byte_count / 1048576 / elapsed, "command": [str(x) for x in command]}
    rows.append(row); (root / "results.json").write_text(json.dumps(metadata, indent=2))
    print(f"{label}/{operation} #{repetition}: {elapsed:.3f}s, peak {peak/1048576:.1f} MiB", flush=True)
    return row

# Generated data: text/source-like files, tiny entries, incompressible bytes and already-compressed bytes.
corpus = root / "mixed"; corpus.mkdir(); (corpus / "empty-dir").mkdir(); (corpus / "empty").touch()
rng = random.Random(20261022)
block = rng.randbytes(1024 * 1024)
with (corpus / "random.bin").open("xb") as out:
    for _ in range(8): out.write(rng.randbytes(1024 * 1024))
with (corpus / "repeated.txt").open("xb") as out:
    line = (b"Zyphie reproducible source text 0123456789\n" * 256)[:8192]
    for _ in range(2048): out.write(line)
(corpus / "compressed.zlib").write_bytes(zlib.compress(block))
for index in range(256):
    (corpus / f"tiny-{index:04}.cpp").write_text(f"// generated {index}\nint value_{index}() {{ return {index}; }}\n")
for path in corpus.rglob("*"): os.utime(path, (1700000000, 1700000000))
os.utime(corpus, (1700000000, 1700000000))
expected = manifest(corpus); input_bytes = sum(item["size"] for item in expected.values())
metadata["datasets"] = {"mixed": expected}
metadata["binaries"] = {key: {"path": str(getattr(args, key)), "sha256": digest(getattr(args, key))} for key in
                        ("zyph", "sevenzip", "zstd", "baseline_zyph") if getattr(args, key)}
tar_path = root / "mixed.tar"
with tarfile.open(tar_path, "w", format=tarfile.USTAR_FORMAT) as archive: archive.add(corpus, arcname="mixed")
variants = [("store-1", ["--store"]), ("fast-1", ["--preset", "fast"]), ("fast-4", ["--preset", "fast", "--threads", "4"]),
            ("balanced-1", []), ("balanced-4", ["--threads", "4"]),
            ("maximum-1", ["--preset", "maximum"]), ("maximum-4", ["--preset", "maximum", "--threads", "4", "--memory-mib", "1024"])]
if args.baseline_zyph: variants.insert(0, ("milestone3-balanced-1", []))
for repetition in range(args.repeats):
    for label, options in variants:
        exe = args.baseline_zyph if label.startswith("milestone3") else args.zyph
        archive = root / f"{label}-{repetition}.zyph"; restored = root / f"{label}-{repetition}-out"
        row = measure(label, "pack", [exe, "pack", corpus, "-o", archive, *options], root, input_bytes, repetition)
        row["archive_bytes"] = archive.stat().st_size
        measure(label, "verify", [exe, "verify", archive], root, input_bytes, repetition)
        measure(label, "list", [exe, "list", archive], root, input_bytes, repetition)
        measure(label, "extract", [exe, "extract", archive, "-o", restored], root, input_bytes, repetition)
        assert manifest(restored / "mixed") == expected
        if not label.startswith("milestone3"):
            selected = root / f"{label}-{repetition}-selected"
            measure(label, "select", [exe, "extract", archive, "-o", selected, "--select", "mixed/tiny-0000.cpp"], root,
                    (corpus / "tiny-0000.cpp").stat().st_size, repetition)
            assert digest(selected / "mixed/tiny-0000.cpp") == digest(corpus / "tiny-0000.cpp")
            assert len(list(selected.rglob("*.cpp"))) == 1
            remove_output(selected)
        remove_output(restored)
    for label, options in [("zip-deflate-5-1", ["-tzip", "-mm=Deflate", "-mx=5", "-mmt=1"]),
                           ("7z-lzma2-5-1", ["-t7z", "-m0=lzma2", "-mx=5", "-ms=off", "-mmt=1"])]:
        archive = root / f"{label}-{repetition}.archive"; restored = root / f"{label}-{repetition}-out"
        row = measure(label, "pack", [args.sevenzip, "a", *options, archive, "mixed"], root, input_bytes, repetition)
        row["archive_bytes"] = archive.stat().st_size
        measure(label, "verify", [args.sevenzip, "t", archive], root, input_bytes, repetition)
        measure(label, "extract", [args.sevenzip, "x", archive, f"-o{restored}"], root, input_bytes, repetition)
        assert manifest(restored / "mixed") == expected; remove_output(restored)
    archive = root / f"native-zstd-{repetition}.zst"; restored = root / f"native-zstd-{repetition}.tar"
    row = measure("native-zstd-3-1", "pack", [args.zstd, "-3", "-T1", tar_path, "-o", archive], root, input_bytes, repetition)
    row["archive_bytes"] = archive.stat().st_size
    measure("native-zstd-3-1", "verify", [args.zstd, "-t", archive], root, input_bytes, repetition)
    measure("native-zstd-3-1", "extract", [args.zstd, "-d", archive, "-o", restored], root, input_bytes, repetition)
    assert digest(restored) == digest(tar_path); restored.unlink()

if args.large:
    for size in (64 * 1024 * 1024, 4 * 1024**3 + 1024 * 1024):
        source = root / f"large-{size}.bin"
        with source.open("xb") as out:
            out.write(b"Zyphie large-file fixture\n")
            out.seek(size - len(block)); out.write(block)
        expected_hash = digest(source)
        metadata["datasets"][source.name] = {"size": size, "sha256": expected_hash}
        for repetition in range(args.repeats):
            label = f"large-{size}-balanced-2"; archive = root / f"{label}-{repetition}.zyph"; restored = root / f"{label}-{repetition}-out"
            row = measure(label, "pack", [args.zyph, "pack", source, "-o", archive, "--threads", "2"], root, size, repetition)
            row["archive_bytes"] = archive.stat().st_size
            measure(label, "extract", [args.zyph, "extract", archive, "-o", restored], root, size, repetition)
            assert digest(restored / source.name) == expected_hash
            remove_output(restored)
        assert digest(source) == expected_hash
metadata["all_roundtrips_verified"] = True
(root / "results.json").write_text(json.dumps(metadata, indent=2))
print(f"Verified results: {root / 'results.json'}", flush=True)
