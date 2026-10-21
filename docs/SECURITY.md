# Security — milestone 2 boundary

The engine treats archive metadata as untrusted, but **milestone 3 hardening has
not been completed**. Do not claim comprehensive safe extraction against an active
local filesystem attacker. Use source/destination parents controlled by the caller.

## Implemented and regression-tested

- Explicit little-endian parsing, checked arithmetic and strict reserved/version
  validation; no native-struct deserialization.
- SHA-256 coverage for footer, header/manifest/index, decoded blocks and whole files.
  These hashes detect corruption, not authenticity.
- Bounded metadata/count/output limits before allocation or output; bounded block
  buffers and Zstandard decoder windows. Exact frame/content-size checks.
- Strict UTF-8 paths, no traversal/absolute/drive/UNC/ADS forms, reserved names and
  path-component validation. Sorted unique entries and explicit directory parents.
- Exact block/file ownership, contiguous bounds, no overlapping/unreferenced bytes.
- Native no-follow file opens, exclusive output creation, symlink/reparse checks,
  no existing destination overwrite, temporary staging and no-replace publication.
- Source size/timestamp checks and rejection of output inside an input directory.

Tests cover every truncation of a fixed archive, trailing data, corrupted payloads,
forged metadata with valid recomputed hashes, unsafe paths, codecs and ranges,
wrong whole-file digests, resource limits, existing-output refusal, symlink paths,
pre-cancelled operations and cleanup after a late packing failure. See project
status for actual local and hosted run results; test names are not security claims.

## Required milestone 3 work

- Handle-relative, race-resistant traversal of directory components and Windows
  reparse points; staging ACL review. Current path checks have TOCTOU gaps if a
  hostile process can mutate source/destination parents between checks and use.
- Dedicated fuzz harnesses for parser and paths, bounded fuzz campaigns and retained
  minimized regressions. No fuzz campaign has run yet.
- Broader Windows device/Unicode alias/case-collision and junction-race tests.
- Disk-full, permissions, flush/rename failures and in-flight cancellation/source
  mutation fault injection. Current pre-cancel tests do not prove all interruption paths.
- Windows-native diagnostics and sanitizer-instrumented dependency builds, beyond
  project-level Linux ASan/UBSan. Assess parser allocation amplification within caps.
- Crash/power-loss durability, recovery and fsync of parent directories. A successful
  rename is not a promise of crash-atomic durable restoration on every filesystem.

No archive repair, encryption or authentication is implemented. A malicious archive
producer can replace unkeyed digests. Directory creation/file identity changes are
not captured through a snapshot service. Metadata not specified by V1 (ACLs,
permissions, streams, ownership, subsecond times) is not preserved. Unsupported
filesystem entry types fail explicitly. Cleanup may leave an isolated `.zyph-part-*`
object if the OS refuses removal; it must never delete original inputs.
