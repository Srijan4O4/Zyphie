# Architecture

## Boundaries

`zyph_core` is a Qt-free C++20 library. `zyph` is the CLI. A separate
`zyph_desktop` target will be added at milestone 5, after engine/security gates.
Today only version reporting exists in the core; the remaining design below
describes planned modules, not delivered functionality.

| Module | Responsibility | Planned location |
| --- | --- | --- |
| Format | Explicit little-endian encoding, bounded decoding, version rules | src/format |
| Compression | STORE/Zstandard block adapters, bounded output | src/compression |
| Filesystem | Deterministic scan, file identity and metadata, unsupported-entry reporting | src/filesystem |
| Archive | Writer, reader, manifest/index, inspection and full verification | src/archive |
| Security | Resource limits, path policy and platform-safe output handles | src/security |
| Concurrency | Bounded workers, backpressure, cooperative cancellation | src/concurrency |
| CLI | Argument parsing, exit codes, text/progress presentation | apps/cli |
| Desktop | Qt Widgets presentation and asynchronous job control | apps/desktop |

Public types will live under `include/zyphie`. Introduce interfaces as their
implementations become testable; do not create empty polymorphic wrappers.

## Planned API and data flow

FilesystemScanner produces deterministic ArchiveEntry records for regular files
and directories. ArchiveWriter accepts PackOptions, progress reporting and a
`std::stop_token`. It reads bounded blocks, selects STORE or Zstandard through
CompressionCodec/CompressionPlanner, and streams to an exclusively created
temporary archive. After manifest/index/footer finalization and structural
validation, publish the archive through platform-aware no-clobber operations.

ArchiveReader validates structure and exposes an ArchiveManifest without
decompressing payloads. ArchiveInspector provides summaries from validated
metadata. ArchiveVerifier decodes every selected block and checks block and
whole-file digests. ArchiveExtractor resolves selected entries, preflights
conflicts and limits, and writes through platform-specific confinement helpers.

PackOptions: sources, destination, preset, block size, resource limits, explicit
overwrite, optional explicit skip policy. ExtractOptions: destination, exact
path/subtree selection, limits and overwrite policy. Skipped or partially
restored entries must appear in structured operation results.

Use a small `ErrorCode` enum and `Error` exception carrying operation/path context
at I/O boundaries; errors cross into CLI/GUI once, never as success values.
Proposed codes: invalid_argument, io, unsupported_feature, corrupt_archive,
resource_limit, unsafe_path, destination_exists, source_changed, cancelled.
Cancellation has its own outcome. Public signatures are finalized in milestone 1.

## Resource and correctness model

Start with one worker and a 4 MiB block target. Memory for file contents is
bounded by block size and concurrency; manifest/index memory has explicit caps
and is proportional to capped entry/block counts. Do not claim constant memory
for arbitrarily many tiny files. All offsets and sizes use checked 64-bit
arithmetic. Serialized fields never depend on struct layout or native endian.

Preserve empty entries, UTF-8 relative paths and supported modification times.
Treat links, reparse points and special files as unsupported until explicitly
designed. Reject output archives inside input trees initially. Detect file
size/timestamp changes before and after reads; this is not a filesystem snapshot.

GUI work runs off the event thread and uses the same engine as CLI. Progress
callbacks must not access widgets directly. Threading begins only after the
single-threaded engine passes losslessness and security gates.
