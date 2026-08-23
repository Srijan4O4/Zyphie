# Zyphie — working notes

Huffman file compressor: a C codec engine wrapped in a web service
(upload → compress → deliver → clean up).

## Build

The C engine lives in `engine/`. There is **no `make` on PATH on this machine** —
only `mingw32-make`:

```bash
cd engine && /c/MinGW/bin/mingw32-make
```

On Linux (CI, Docker) it is plain `make`. Profiles keep objects in separate
trees (`build/dev`, `build/release`, `build/debug`) so switching never links
objects compiled with mismatched flags.

| Target | Flags |
|---|---|
| `make` | `-O2 -g` |
| `make release` | `-O3 -DNDEBUG`, stripped |
| `make debug` | ASan + UBSan — **Linux only**, MinGW ships no sanitizer runtime |
| `make test` | build + run suite (wired up in Phase 2) |

Audit pass for implicit narrowing in the bit-packing code:
`make CFLAGS_EXTRA=-Wconversion`. It is off by default because it is too noisy
to combine with the `-Werror` CI uses.

## Which document wins

`docs/format-spec.md` is **authoritative**. It is newer and far more specific
than `file-compressor-system-design.md`, and it defines the on-disk format,
which is the least reversible decision in the project. Where they conflict:

- Output is **`.zy`** with magic `ZYPH` — *not* the design doc's `.huff`.
- The binary is `zyphie`, not `compressor`.
- The design doc describes the engine as already existing and "solid." It did
  not; it is being built in Phases 0–3.

Other known errors in the design doc, corrected as those phases land: its
`docker-compose.yml` omits Postgres (§9 requires it), its Dockerfile's `gcc`
source list omits `crc32.c` and `container.c`, and its §5.5 worker sample
leaks a dangling multipart upload on engine failure.

## Invariants that are easy to break silently

- **Symbols are `uint8_t`, never `char`.** `char` is signed on x86, so bytes
  ≥ `0x80` index a `freq[256]` table negatively. The format uses all 256 literals.
- **Huffman tie-breaking must be deterministic.** Equal frequencies extracted
  in an arbitrary order give different tree shapes → different code lengths →
  different output bytes, which violates format-spec §1's byte-identical-output
  guarantee. Compare `(freq, seq)`: leaves `seq = symbol`, internal nodes
  `seq = 256 + creation_index`.
- **stdin/stdout must be set to binary mode on Windows** via
  `_setmode(_fileno(...), _O_BINARY)`. Text mode rewrites `\n` → `\r\n` and
  treats `0x1A` as EOF, corrupting every stream. The whole web layer depends on
  the engine being a clean binary filter.
- **Never `fwrite` a struct to the stream.** All integers are serialised
  byte-by-byte, little-endian, with explicit shifts (format-spec §2). Writing
  structs embeds host endianness and compiler padding into the file format.
- **Use `uint64_t`, not `size_t`, for stream-length accumulators.** The local
  target is 32-bit (`gcc -dumpmachine` → `mingw32`) where `size_t` is 4 bytes,
  but the format's length fields are 8.
- **Golden test fixtures are never regenerated.** Regenerating them is how a
  project breaks backward compatibility with every test still green
  (format-spec §10).

## Correctness environment

Local gcc is MinGW.org 6.3.0 targeting 32-bit `mingw32`, with **no
AddressSanitizer**. Linux CI is therefore the canonical gate for sanitizers and
decoder fuzzing. The 32-bit/64-bit split is useful rather than merely awkward —
it catches word-size assumptions early.

## Architectural rule

The engine is a trusted, stateless Unix filter: bytes in on stdin, bytes out on
stdout. It is never taught about HTTP, S3, or queues, and it is always invoked
as a **subprocess**, never linked in-process — it is memory-unsafe C parsing
untrusted user files, so a crash must cost one job, not the whole worker.
