# ZYPH V1 binary specification

Status: V1.0 milestone 1 specification. This document defines the wire format;
implementation is pre-release and has not passed the milestone 3 security gate.
All offsets are absolute from byte zero. Integers are unsigned little-endian
unless explicitly signed; signed integers use two's complement. There is no
alignment padding, native struct serialization, varint, encryption or authentication.

## Physical order

`header[32] | payload blocks | manifest | block index | footer[128]`

Blocks occupy consecutive ranges beginning at byte 32, in index ordinal order.
There are no gaps, overlapping ranges or unreferenced payload bytes. The manifest
immediately follows the final block (or header if zero blocks); the index follows
the manifest; the footer follows the index and ends at EOF. Trailing data is invalid.
All additions/multiplications must be checked before seeking or allocating.

## Header (32 bytes)

| Offset | Width | Meaning |
| --- | --- | --- |
| 0 | 8 | Magic: hexadecimal `5a 59 50 48 0d 0a 1a 0a` |
| 8 | 2 | Major: 1 |
| 10 | 2 | Minor: 0 |
| 12 | 4 | Header length: 32 |
| 16 | 4 | Required feature flags: 0 |
| 20 | 2 | Digest identifier: 1 = SHA-256 (32 bytes) |
| 22 | 2 | Reserved: 0 |
| 24 | 4 | Maximum decoded block size, 1 through 16,777,216 bytes |
| 28 | 4 | Reserved: 0 |

Default writer block size is 4,194,304 bytes. All nonfinal blocks of a file need
not be full-sized. A block cannot be empty. STORE codec ID is 0, Zstandard is 1.
A reader rejects unknown major/minor versions, digest IDs, required flags,
nonzero reserved values or wrong fixed lengths as unsupported/invalid. V1.0 has
no optional fields. Future extension requires an explicitly specified version;
a future reader must not reinterpret a V1 field. Never silently ignore features.

## Data blocks and block index

Data blocks have no inline headers. Their complete decoding information is in
the index; the implicit block ID is its zero-based index ordinal (u64).
Index header: 8 ASCII bytes `ZYPHIDX1`, then a u64 block count.
Exactly that many 56-byte records follow, without padding or trailing bytes.

| Record offset | Width | Meaning |
| --- | --- | --- |
| 0 | 8 | Absolute payload offset |
| 8 | 4 | Encoded size; nonzero, no greater than decoded size |
| 12 | 4 | Decoded size; nonzero, no greater than header block limit |
| 16 | 1 | Codec: STORE 0 or Zstandard 1 |
| 17 | 1 | Flags: 0 |
| 18 | 2 | Reserved: 0 |
| 20 | 32 | SHA-256 of decoded bytes, standard digest byte order |
| 52 | 4 | Reserved: 0 |

STORE requires equal encoded/decoded lengths and stores exact original bytes.
Zstandard contains exactly one normal frame, with content size present and equal
to decoded size, no dictionary, no skippable/concatenated frames, and decoder window
at most 16 MiB. Writers use STORE if compression is not smaller than the input.
The block SHA-256 is mandatory even if the Zstandard frame has its own checksum.
Unknown codec IDs are rejected during structural parsing, before extraction.

## Manifest

Header: 8 ASCII bytes `ZYPHMAN1`, then u64 entry count. Each entry consists of the
following 72-byte prefix followed immediately by `path_length` bytes of UTF-8.
No NUL terminator, padding, optional fields or trailing bytes are allowed.

| Entry offset | Width | Meaning |
| --- | --- | --- |
| 0 | 1 | Type: 1 regular file, 2 directory |
| 1 | 1 | Flags: bit 0 = modification time present; other bits 0 |
| 2 | 2 | Reserved: 0 |
| 4 | 4 | Path length in UTF-8 bytes |
| 8 | 8 | Original file size |
| 16 | 8 | Signed Unix modification time in whole seconds, or 0 if absent |
| 24 | 8 | First block ordinal |
| 32 | 8 | Number of consecutive blocks |
| 40 | 32 | SHA-256 of complete original file contents |
| 72 | variable | Relative UTF-8 path |

Entries are strictly sorted by unsigned UTF-8 byte lexicographic order. Identical
paths are forbidden. Every non-root parent path must occur as a directory entry.
A directory has size, first block and block count all zero and 32 zero digest
bytes. A file's block range starts at the next unused block ordinal while walking
manifest order; its decoded sizes sum exactly to its size. Every block belongs to
exactly one file. Empty files consume zero blocks, record the next unused ordinal,
and use SHA-256(empty). All block ordinals must be consumed at manifest end.
An archive with zero entries/blocks is valid. Empty directories are explicit.

Modification times, if present, range from -2,147,483,648 to 4,102,444,800 Unix
seconds inclusive (1901 through 2100). Writers floor subsecond times, never round.
Readers restore second precision where the filesystem permits it, or report an
error. Creation/access times, permissions, ownership, ACLs, extended attributes,
alternate streams, sparse layout and hard-link identity are not represented.
Hard links are read as separate regular files. Links, reparse points, devices,
sockets and other special entries are unsupported; default writer behavior is
an error. No silently skipped entries. V1 writer does not offer a skip option.

## Paths

Path length: 1..4096 bytes. Each component: 1..255 UTF-8 bytes. `/` separates
components. UTF-8 must encode Unicode scalar values, with no overlong encodings,
surrogates or values above U+10FFFF. No Unicode normalization is performed.
Reject empty components, `.` and `..`, leading/trailing slash, backslash, ASCII
controls U+0000..001F and U+007F, and any of `< > : " | ? *`. A component cannot
end in ASCII dot or space. Reject Windows device basenames case-insensitively,
even before an extension: CON, PRN, AUX, NUL, CONIN$, CONOUT$, COM1..COM9, LPT1..LPT9,
and COM/LPT followed by superscript 1, 2 or 3. Thus drive-qualified, UNC, ADS and
device namespace forms cannot be represented. Directory paths have no trailing slash.

Archive identity is case-sensitive. Writers store each input under its basename;
a directory includes that basename and all descendants. Duplicate basenames are
an error. Extraction must reject names that collide on the destination filesystem
(including case/Unicode aliases), as well as existing file/directory conflicts.
Archive strings are not OS paths: convert valid UTF-8 explicitly to native paths.

## Footer (128 bytes)

| Offset | Width | Meaning |
| --- | --- | --- |
| 0 | 8 | ASCII `ZYPHEND1` |
| 8 | 8 | Manifest absolute offset |
| 16 | 8 | Manifest byte length |
| 24 | 8 | Index absolute offset |
| 32 | 8 | Index byte length |
| 40 | 8 | Complete archive byte length, including footer |
| 48 | 16 | Reserved: all zero |
| 64 | 32 | SHA-256(header bytes || manifest bytes || index bytes) |
| 96 | 32 | SHA-256 of footer bytes 0..95 inclusive |

Each section is at least 16 bytes; minimum archive length is 192 bytes. Locate
footer at EOF-128, verify its digest, validate section bounds and relationships,
then read the header/manifest/index and verify their combined digest. The header
is always the first 32 bytes. Payload SHA-256 values are covered by the metadata
digest; payloads themselves are read only for extraction/full verification.

Structural validation checks all syntax, versions, reserved fields, path rules,
counts, ranges, metadata digests and mappings. Full verification additionally
decodes every block, checks its digest, and checks each reconstructed file digest.
Listing performs structural validation without decompressing payloads. None of
these unkeyed hashes proves authenticity: an attacker can recompute them.

## Reader resource limits

Default limits: 100,000 entries; 1,000,000 blocks; 64 MiB combined manifest/index
bytes; 1 TiB per file; 4 TiB aggregate original bytes; 8 TiB archive length.
Name and block limits above are format limits. Configurable policy limits may be
lowered or raised within representable OS sizes before parsing; exceeding them is
a resource-limit error, never a reason to allocate first. Metadata memory is
bounded by metadata/count limits; payload processing uses bounded block buffers.

## Determinism and reference vectors

Equivalent ordered entries, timestamps, block partition and codec output produce
identical bytes. Zstandard output may differ between codec releases; the decoder
contract and reconstructed bytes remain stable. No timestamps of archive creation,
random IDs or machine-specific fields occur in the format.

Header vector for a 4 MiB limit (hex):
`5a5950480d0a1a0a010000002000000000000000010000000000400000000000`

SHA-256(empty):
`e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855`

SHA-256(ASCII abc):
`ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad`

Fixed reference archives under tests/fixtures are generated by a separate,
standard-library-only Python reference encoder, never by the C++ writer. Its
purpose is test-fixture generation, not a runtime/backend dependency. Vectors
include empty archive/file/directory, STORE abc, and a fixed Zstandard RLE frame for 64 ASCII a bytes.
Tests must reject truncation at each boundary, bad reserved values/versions,
invalid UTF-8, corrupted payload/metadata, unsafe paths and inconsistent lengths.
Reference encoder source and hashes must accompany the archives.

Milestone 3 changes only filesystem enforcement and validation; all V1 byte layouts
and golden archive compatibility remain unchanged. Extraction creates private
native permissions rather than restoring permissions not represented by V1.

Milestone 4 adds ordered worker scheduling and selective extraction without a wire
change. A selector is an exact case-sensitive manifest path, with directories
expanding to descendants and ancestors retained. Metadata is validated globally;
payload verification applies only to selected files unless full verify is requested.

Milestones 5 and 6 retain the same V1 layout and golden vectors. Both desktop and
CLI use this reader/writer. Product version 1.0.0 does not change the wire version.
