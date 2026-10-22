# Security model — milestone 3

Treat all archive bytes and names as hostile. Supported native operations target
Windows local filesystems and Linux filesystems with no-replace rename support.
The archive format has not changed. Final milestone evidence is in PROJECT_STATUS.md.

## Confinement and writes

- Validate UTF-8 and all path components before output: reject absolute, drive,
  UNC, ADS, traversal, reserved-device and trailing-dot/space forms. Require sorted
  unique archive paths and explicit directory parents. Native name collisions
  fail without publishing a partial tree, including Windows case aliases.
- Open Windows ancestors one component at a time using NtCreateFile RootDirectory
  handles and FILE_OPEN_REPARSE_POINT; reject reparse handles. Keep directory
  handles open without delete sharing. Resolve file creation, directory creation,
  timestamp changes, cleanup and no-replace rename relative to those handles.
  The ancestor-swap and junction-mutation regressions test this behavior directly.
- On Linux, walk ancestors through openat with O_DIRECTORY/O_NOFOLLOW. Use
  openat(O_EXCL/O_NOFOLLOW), mkdirat, futimens, renameat2(RENAME_NOREPLACE) and
  descriptor-relative cleanup. A concurrently renamed ancestor remains the opened
  directory object; replacement symlinks cannot redirect that native operation.
- Create Windows outputs with a protected owner/SYSTEM DACL and Linux directories
  with mode 0700/files 0600 at creation. Do not expose staging under inherited
  broad permissions and tighten it later. These private permissions remain on the
  published output; original permissions/ACLs are not archive metadata.
- Stage output under a random exclusive name, flush files, verify payload/file
  hashes and publish only on success. Existing destinations are never replaced.
  Cleanup unlinks reparse/symlink objects themselves without traversing targets.

The threat boundary excludes privileged or same-identity processes that can alter
our private objects or process memory. Callers should control their output parent;
opening a directory pins an object, not an immutable pathname on Linux. Concurrent
changes may cause a controlled failure and leave isolated staging if cleanup is
denied. This is not a filesystem snapshot, sandbox, or protection against hostile
kernel/filesystem implementations. Source size/timestamps are checked before and
after reads; an attacker able to restore metadata can evade snapshot-like guarantees.

## Parsing and resource bounds

Explicit little-endian fields, checked additions/multiplications, version/reserved
validation, contiguous block ownership and exact section sizes are checked before
use. SHA-256 covers footer, metadata, decoded blocks and complete files. Digests
provide corruption detection, not authenticity; an attacker can recompute them.
Zstandard frames must have the expected content size, one complete frame, no
external dictionary and a bounded decoder window.

Defaults: 64 MiB encoded metadata, 100,000 entries, 1,000,000 blocks, 16 MiB maximum
block/window, 1 TiB per file, 4 TiB total decoded output and 8 TiB archive. Metadata
buffers, decoded objects, path-map copies and allocator overhead coexist, so the
metadata cap is not an RSS cap. Object counts and path lengths independently bound
allocation amplification; no allocation is based on an unchecked archive count.
These output quotas are configurable by the trusted caller. Large-file performance
and measured memory consumption remain milestone 4 work.

## Validation

The regression suite includes every truncation of a golden archive, trailing data,
forged offsets/codecs/paths with recomputed checksums, invalid UTF-8, resource limits,
corrupt blocks and whole-file digests. It tests late failures, existing outputs,
publication races, ancestor swaps, symlink cleanup, private ACLs/modes, case
collisions, in-flight cancellation and changing source data. Windows-only tests
exercise junction mutation and handle counts after repeated parser failures.

Internal thread-local hooks inject write, flush, publication and directory-create
failures. They model failed disk/permission/publication operations without filling
or damaging the host filesystem. Hooks are absent when BUILD_TESTING=OFF and are
never controlled by archive contents or environment variables. Native I/O errors
remain explicit; failures do not masquerade as successful restoration.

Two Linux Clang libFuzzer targets run with ASan/UBSan: production archive
parsing/verification (including a checksum-repaired pass) and path validation.
See tests/fuzz/README.md and the workflow for limits, seeds and exact commands.
The bounded campaigns are evidence, not proof of absence of bugs.

## Additional release assurance and limitations

Long-running fuzzing, fully instrumented codec/crypto dependencies, Windows
Application Verifier/PageHeap, real disk-full/power-loss experiments and broader
filesystem/network-share matrices have not been executed. Linux sanitizers
instrument project targets; dependency internals are not instrumented. Fault
injection does not validate every OS failure mode. These are explicitly retained
release-assurance tasks, not completed test claims.

Publication is no-replace but not a crash-durability guarantee: directory fsync,
power-loss recovery and automatic recovery of abandoned staging are not provided.
No encryption, authentication, repair, ACL/stream/ownership preservation or release
security certification is claimed. Do not remove abandoned output automatically
without establishing ownership; never delete original inputs to recover space.

## Milestone 4 concurrency and cancellation

The worker pool never shares archive I/O handles or progress callbacks. Buffered
work is capped by worker count; memory admission covers blocks/codec workspaces,
not metadata or total RSS. Selected extraction retains all M3 path/publication
checks and verifies selected file/block hashes; unrelated payloads are not read.
An actual CLI signal integration test supplements API cancellation regressions.
Windows publication can retry transient access/sharing failures for up to eight
25 ms waits at each of its open and rename steps; each attempt checks cancellation
and retains no-replace behavior. Permanent failures still return an error.
