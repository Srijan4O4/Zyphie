# Security design status

No archive parser or extractor exists yet. No claim of hardened archive handling
is made. The future engine treats every archive, path and declared length as
untrusted; it must meet milestone 3 before GUI work or distribution.

## Threats and required controls

- Bounds/overflow: validate section ranges and checked offset arithmetic before
  reading, allocating or decoding; reject overlaps, missing bytes and trailing data.
- Resource exhaustion: cap entry count, path bytes, manifest/index bytes, block
  size, file output and aggregate output before allocation or writes. A ratio
  threshold alone is insufficient. Explicit output limits must permit legitimate
  large files when deliberately configured.
- Paths: strict UTF-8, relative slash-separated components; reject dot segments,
  absolute/drive/UNC/device forms, NUL, ADS syntax, Windows reserved names and
  trailing-dot/space aliases. Detect case-insensitive collisions and file/directory
  ancestor conflicts before writing.
- Filesystem races: lexical checks and canonical-prefix comparisons alone do not
  prevent junction/symlink substitution. Design platform handle-based confinement
  and exclusive creation; test reparse points and concurrent path changes on Windows.
- Data integrity: verify authenticated structure only if an authenticated mechanism
  is actually implemented. V1 uses unkeyed corruption detection, not authenticity.
  Full verification must reconstruct and digest file contents, not just parse a header.
- Output safety: exclusive temporary creation, no unauthorized overwrite, validate
  before publication, accurate partial-result/cancellation reporting, and cleanup
  limited to temporary objects owned by this operation.

## Validation required before security gate

Malformed/truncated headers, records, manifest, index and footer; unsupported
versions/features/codecs; reserved bits; digest mismatches; overlapping block
ranges; count/offset overflows; impossible lengths; invalid UTF-8; duplicate and
alias paths; traversal; device names; symlink/reparse traversal; disk-full/permission
failures; cancellation; changing source files; and finalization failures.

Add independent parser/path fuzz targets and preserve minimized regressions.
Run ASan/UBSan on Linux, Windows functional tests and appropriate Windows-native
diagnostics. Fuzzing elapsed time, corpus and sanitizer results must be recorded.
No tests above have run at this foundation checkpoint.
