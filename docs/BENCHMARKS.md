# Benchmarks

**Not measured.** The milestone 2 engine has not been benchmarked. No speed,
compression-ratio or memory-efficiency claim is supported at this checkpoint.

At milestone 4, build a runner recording revision, compiler/options, codec
versions, CPU/RAM, storage/filesystem, OS, block size, thread count and preset.
Record exact commands and dataset SHA-256 identifiers. Keep large datasets out
of Git. Use generated/public redistributable corpora; never private user data.

Corpus: source trees, text, binary executables, creator assets, already-compressed
media, mixed directories, tiny files, empty entries, incompressible random bytes
and files exceeding 4 GiB. Separate cold/warm cache conditions and report repeated
runs, median and variation.

Measure pack/extract/verify time, archive bytes, peak working set, CPU time and
selective extraction latency. Verify byte equality before accepting a run.
Compare ZIP/Deflate, 7-Zip and native Zstandard with exact settings and comparable
thread budgets. Report container overhead and differences in metadata/solid mode.
Profile before optimizing and retain baseline and changed measurements.
