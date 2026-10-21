# ZYPH format V1 — design pending

**Status: milestone 0 design checklist, not a binary specification.** No byte layout
is frozen, no valid `.zyph` archive is produced, and no compatibility promise exists.
Implementing a serializer against this checklist is prohibited. Milestone 1 must
replace it with exact layouts and test vectors before archive implementation.

## Required design work

1. Header: exact magic, byte offsets, fixed-width little-endian fields, major/minor
   version, lengths, flags, digest identifiers and reserved-value validation.
2. Independent block record: ID, codec (STORE/Zstandard initially), flags, encoded
   and decoded sizes, payload location and digest. Specify hard bounds and exact
   checksum coverage; define fallback when compression expands a block.
3. Typed manifest: UTF-8 relative path, file/directory type, original size, ordered
   file/block mapping, whole-file digest, timestamp epoch/unit/range, supported
   metadata and empty entry rules. Specify variable-length field encoding and
   uniqueness/canonical ordering without locale-dependent collation.
4. Index: exact record sizes, lookup ordering, section relationships, bounds and
   overlap rules. Listing must not decode payloads. Each file's block mapping must
   reconstruct its exact declared length with no missing or duplicate ranges.
5. Footer: exact signature, index/manifest location and sizes, complete archive
   size, integrity coverage and incomplete/trailing-data rejection rules.
6. Compatibility: required/optional field mechanism, unknown major/minor policy,
   unknown required flags/codecs rejection, reserved bits and deterministic output.
7. Resource policy: default and hard maxima for every count, field, section,
   block, file, total output and decoder window; checked arithmetic throughout.
8. Paths: Unicode encoding policy, slash separator, case-sensitive archive identity,
   Windows aliases/collisions, reserved names, drive/UNC/ADS/device forms, empty
   components, illegal characters and extraction behavior across filesystems.
9. Filesystem semantics: directories and regular files; hard links stored as normal
   file bytes; explicit unsupported-entry errors or requested skip summaries.
10. Integrity: select a maintained digest implementation, assign IDs, specify
    coverage including metadata, define empty-file digest and verification order.
    Unkeyed digests detect corruption; they do not authenticate the producer.

## Milestone 1 acceptance

Publish byte-offset tables and independent golden vectors for an empty archive,
an empty file, an empty directory, a STORE block and a Zstandard block. Include
malformed examples and expected errors. An engineer must be able to implement a
compatible reader using this document alone. Encode/decode tests, size arithmetic
tests, version rejection and codec integration must actually pass.
