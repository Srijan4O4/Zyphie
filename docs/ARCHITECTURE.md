# Architecture — milestone 2

`zyph_core` is a Qt-free C++20 static library. `zyph` is a thin CLI using that same
engine. The future desktop application must use the public archive API without
duplicating compression logic. The desktop target remains disabled until its gate.

## Implemented boundaries

| Location | Responsibility |
| --- | --- |
| include/zyphie/archive.hpp | ArchiveReader, ArchiveWriter, ArchiveEntry, Limits, PackOptions |
| include/zyphie/binary.hpp | Explicit little-endian fields, bounded reads, checked arithmetic |
| src/format/format.cpp | Header/footer serialization, version checks, UTF-8/path policy |
| src/core/digest.cpp | Incremental SHA-256 using BCrypt or OpenSSL EVP |
| src/compression/codec.cpp | Bounded independent Zstandard compression/decompression |
| src/filesystem/io.cpp | Native exclusive output creation, read handles, no-replace publication |
| src/archive/archive.cpp | Deterministic scanner, manifest/index parser, writer, verification/extraction |
| apps/cli/main.cpp | Unicode argument conversion, command/options, text output, exit codes |

The scanner and manifest/index routines remain internal helpers in the archive
module at this increment. Their responsibilities are separate functions; no empty
interface hierarchy is introduced. Inspection is exposed through reader metadata,
and verification/extraction through methods sharing the same validated index.
The reader owns one open native file handle; its operations are synchronous and
not concurrently callable on the same instance. Reader copying is disabled.

## Write pipeline

Scan regular files/directories without following links; reject unsupported entries
and duplicate internal names. Bound entries, metadata and total sizes. Sort by
UTF-8 byte order. Reject the destination inside any source directory. Stream one
block at a time through STORE or Zstandard and compute block/whole-file SHA-256.
Write payloads to an exclusively created temporary file in the destination parent.
Record manifest/index, protect header+metadata and footer, flush/close, reopen
and fully verify, then publish with a no-replace rename on Windows/Linux.

Check file sizes and timestamps before/after reads and tree timestamps before
finalization. These detect many changes but do not implement a filesystem snapshot.
Input files are read-only. Temporary cleanup owns only objects created by this job.
No final archive name appears until validation succeeds.

## Read/extract pipeline

Read EOF footer, validate its hash and section arithmetic before allocations.
Enforce metadata/count limits, verify metadata digest, parse index and typed
manifest, require sorted unique valid paths and explicit directory parents.
All blocks must map exactly once to files with matching decoded lengths.
Listing exposes this metadata without payload decompression.

Verification reads each referenced block with bounded buffers, constrains the
Zstandard frame/window, validates block SHA-256, then validates whole-file SHA-256.
Extraction builds a new staging tree under the destination parent, creates each
output exclusively, verifies file contents, restores second-precision timestamps
and publishes the complete tree with no replacement. Any error/cancellation leaves
the final destination absent; owned temporary outputs are cleaned where possible.

## Errors and resource model

Public operations throw Error with ErrorCode. Filesystem errors translate to I/O
errors; corruption, unsupported features, limits, paths, conflicts, changing inputs
and cancellation are distinct categories. CLI maps them to documented exit codes.
No failed/skipped entry is reported as a successful pack or complete extraction.

Content buffers are bounded by configured block size (hard maximum 16 MiB). Metadata
is bounded by policy and scales with capped entry/block counts; encoded metadata,
parsed vectors and scanner paths coexist, so the metadata byte cap is not a total
process-memory cap. Default payload block size is 4 MiB. Compression is single
threaded. Basic stop-token checks occur at file/block boundaries and before publish;
worker pools, progress and comprehensive interruption tests belong to milestone 4.

## Remaining security boundary

Native no-follow/exclusive file opens and repeated path checks reject ordinary
symlinks/reparse points and overwrite conflicts. They do not yet provide full
handle-relative directory traversal or Windows staging ACL isolation. Destination
parents must be trusted against concurrent hostile mutation. See SECURITY.md;
passing milestone 2 does not imply milestone 3 or release readiness.
