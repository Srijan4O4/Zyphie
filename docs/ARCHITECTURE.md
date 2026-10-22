# Architecture — V1

`zyph_core` is a Qt-free C++20 static library. `zyph` is a thin CLI using that same
engine. The Qt Widgets desktop uses the public archive API without
duplicating compression logic. ZYPH_BUILD_DESKTOP is optional and defaults off for core-only builds.

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
UTF-8 byte order. Reject the destination inside any source directory. Stream bounded
blocks through STORE or Zstandard and compute block/whole-file SHA-256.
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
process-memory cap. Default payload block size is 4 MiB. Compression uses 1–16 configurable workers with bounded admission (see below). Stop tokens and progress callbacks cover file/block boundaries and publication.

## Milestone 3 native filesystem boundary

The filesystem module now resolves mutation paths relative to checked native
directory handles, applies private staging permissions at creation and publishes
with no-replace native renames. Windows uses ntdll directory-relative APIs and
handle-relative cleanup; Linux uses the *at family and descriptor-relative cleanup.
The wire format and public archive API remain unchanged. See SECURITY.md for the
object-confinement boundary, failure semantics and remaining release assurance.

## Milestone 4 pipeline

A fixed jthread pool compresses independent owned blocks. The caller thread scans,
reads, maintains whole-file hashes and writes completed results in archive order.
At most 2*threads futures (one in serial mode) retain raw/encoded buffers; admission
uses a codec-context estimate and conservative buffer allowance. Backpressure is
applied before another block is read. Worker exceptions are transported through
futures; stopping joins active work and discards queued work. Codec calls are not
preempted. Metadata remains bounded by its separate configured limits.

Progress callbacks execute on the caller thread and use independent scan, pack,
verify and extract counters. Callback exceptions abort before publication. CLI
Ctrl+C handling sets an atomic flag; a stop-aware monitor requests cancellation.
Windows publication retries only bounded sharing/access failures, retaining native
parent handles and no-replace semantics through every attempt.

Selected extraction validates the full manifest/index, binary-searches exact path
selections, adds directory descendants and necessary ancestors, then decodes only
selected file blocks. Unselected payload integrity is deliberately not asserted.
The format, metadata order and golden archive compatibility are unchanged.

## Desktop boundary

apps/desktop links Core/Gui/Widgets dynamically; no Qt header enters zyph_core.
Each job has one jthread that owns its reader/writer and copied input parameters.
A timer on the GUI thread polls a mutex-protected latest progress/result snapshot;
workers never access widgets and cannot flood the event queue. Stop requests flow
through existing core tokens. Closing during work requests cancellation and keeps
the window alive; destruction joins the worker before destroying its shared state.
Errors are displayed as plain text. Archive-path changes clear stale selections.
Presets and exact selections map directly onto the existing API. A successful
browse is explicitly distinguished from full payload verification.
