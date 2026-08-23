# Zyphie Container Format Specification

**Status:** Draft v1 · **Applies to:** `zyphie` engine · **Last revised:** 2026-08-22

This is the least reversible decision in the project. Once a user has a `.zy` file on disk, the decoder must be able to read it forever. Everything here is designed so that v1 files remain decodable after the v2 LZ77 stage ships.

---

## 1. Design goals, in priority order

**Correctness above all.** `decompress(compress(x)) == x` for every possible byte sequence `x`, including empty input. This is verified by checksum at runtime and by fuzzing in CI, not by inspection.

**Never meaningfully expand input.** A compressor that grows a JPEG is broken as a product regardless of how good its entropy coder is. The stored-block escape hatch (§5.1) bounds worst-case growth to 9 bytes per block — about 0.0009% at the default block size.

**Streamable in one pass, in both directions.** The encoder must work as a Unix filter with no knowledge of total input length, because the web layer pipes object storage straight into stdin. This is why total size lives in the footer, not only the header.

**Independently decodable blocks.** Each block carries everything needed to decode itself. This buys parallel compression, parallel decompression, and resumability, at the cost of a small ratio loss at block boundaries (§8).

**Portable and endian-safe.** The same source must compile for x86-64, ARM64, and WebAssembly and produce byte-identical output. No struct-punning, no host-endian writes.

---

## 2. Conventions

These two rules are the most common source of "works on my machine" corruption bugs, so they are stated before anything else.

**Integers.** All multi-byte integers are unsigned little-endian, fixed width, with no alignment padding. They MUST be serialised byte-by-byte with explicit shifts:

```c
static void put_u32le(FILE *f, uint32_t v) {
    fputc((int)( v        & 0xFF), f);
    fputc((int)((v >>  8) & 0xFF), f);
    fputc((int)((v >> 16) & 0xFF), f);
    fputc((int)((v >> 24) & 0xFF), f);
}
```

Writing a `struct` directly with `fwrite(&hdr, sizeof hdr, 1, f)` is forbidden. It embeds the host's endianness and the compiler's padding choices into the file format, and it will silently produce incompatible files on ARM or under Emscripten.

**Bit order.** Within the entropy-coded payload, bits are packed **most-significant-bit first**. The first code emitted occupies the high bits of the first byte. The final byte of a payload is zero-padded in its low bits. (DEFLATE uses LSB-first; Zyphie deliberately does not, because MSB-first is far easier to verify by eye in a hex dump during development.)

**Sizes.** `KiB` = 1024 bytes, `MiB` = 1048576 bytes.

---

## 3. Stream layout

```
+--------------------+
|  Stream header     |  16 bytes, fixed
+--------------------+
|  Block 0           |  variable
+--------------------+
|  Block 1           |  variable
+--------------------+
|  ...               |
+--------------------+
|  End-of-stream     |  1 byte  (0xFF)
+--------------------+
|  Stream footer     |  12 bytes
+--------------------+
```

Fixed overhead is 29 bytes per file plus per-block framing. For a single 1 MiB block that is under 0.003% of the input.

---

## 4. Stream header (16 bytes)

| Offset | Size | Field | Value |
|---|---|---|---|
| 0 | 4 | `magic` | ASCII `Z`,`Y`,`P`,`H` = `5A 59 50 48` |
| 4 | 1 | `format_version` | `1` = Huffman only, `2` = LZ77 + Huffman permitted |
| 5 | 1 | `flags` | bit 0 = footer checksum present; bits 1–7 reserved, MUST be 0 |
| 6 | 1 | `block_size_log` | log2 of the uncompressed block size; valid range 16–24; default 20 (1 MiB) |
| 7 | 1 | `reserved` | MUST be 0 |
| 8 | 8 | `original_size` | Total uncompressed length, or `0xFFFFFFFFFFFFFFFF` if unknown at write time |

`original_size` exists so a decoder can pre-allocate and so a UI can show a progress bar. The all-ones sentinel is what makes true streaming possible: when compressing from stdin the encoder does not know the length, writes the sentinel, and records the real value in the footer instead.

A decoder MUST reject the stream if `magic` mismatches, if `format_version` exceeds what it supports, if `block_size_log` is outside 16–24, or if any reserved bit is set. Rejecting unknown reserved bits is what keeps the format extensible — it means a future v3 file fails loudly on a v2 decoder instead of decoding to garbage.

---

## 5. Blocks

Every block begins with a 9-byte frame:

| Offset | Size | Field | Meaning |
|---|---|---|---|
| 0 | 1 | `block_type` | see table below |
| 1 | 4 | `raw_len` | uncompressed bytes this block represents |
| 5 | 4 | `enc_len` | bytes of payload that follow this frame |

| `block_type` | Name | Introduced | Payload |
|---|---|---|---|
| `0x00` | STORED | v1 | `raw_len` literal bytes, uncoded |
| `0x01` | HUFFMAN | v1 | Canonical static Huffman over 256 literals (§6) |
| `0x02` | LZ77_HUFFMAN | v2 | LZ77 tokens, entropy coded (§7) |
| `0x03` | RLE | v1 | Exactly 1 byte: the single distinct value in this block |
| `0xFF` | END_OF_STREAM | v1 | Terminator; `raw_len` and `enc_len` fields are omitted |

Constraints a decoder MUST enforce before allocating anything: `raw_len` ≤ `1 << block_size_log`, `enc_len` ≤ `raw_len + 128 + 16` for coded blocks, and for STORED `enc_len == raw_len`. Validating these *before* `malloc` is what prevents a 12-byte malicious header from requesting a 4 GiB allocation.

### 5.1 STORED — the escape hatch

The encoder computes the coded size first. If coded size ≥ `raw_len`, it discards the coded output and emits STORED instead. Worst case per block becomes 9 bytes of frame overhead rather than genuine expansion.

Measured on 1 MiB of `/dev/urandom`: naive Huffman with no fallback yields **+0.012%** growth; with the fallback, **+0.0009%**. The absolute numbers are small, but the guarantee is what matters — it lets the product promise "we will never make your file bigger" and mean it.

### 5.2 RLE — turning an edge case into a feature

A block containing exactly one distinct byte value is emitted as RLE.

This exists for a subtle reason beyond compression. A single-symbol Huffman alphabet produces one code of length 1, so `Σ 2^-len = 0.5 ≠ 1` — an *incomplete* prefix code. Incomplete codes are legal but are a notorious decoder-bug breeding ground, because the standard canonical-decode loop can consume the undefined branch. Routing this case to its own block type means the Huffman decoder can unconditionally require a **complete** code (§6.3) and reject anything else, which is a much stronger and simpler invariant.

The compression win is a bonus: a 1 MiB run of zero bytes becomes 10 bytes.

---

## 6. HUFFMAN block payload (v1)

```
+---------------------------+
| code_lengths[256]         |  128 bytes: 256 x 4-bit lengths, two per byte,
|                           |  symbol 2k in the HIGH nibble, 2k+1 in the LOW nibble
+---------------------------+
| bit-packed symbol codes   |  enc_len - 128 bytes, MSB-first
+---------------------------+
```

A code length of `0` means the symbol does not occur in this block. Lengths are 0–15, which is exactly what fits in a nibble — the 4-bit field and the 15-bit code limit are the same constraint viewed twice.

Header cost is a flat 128 bytes, or 0.012% of a 1 MiB block. DEFLATE compresses its own code-length table with a secondary Huffman code to save roughly 60 bytes per block. That is a 0.006% gain for a meaningful amount of additional complexity and bug surface, so Zyphie deliberately does not do it.

### 6.1 Code lengths MUST be limited to 15 bits

This is not a theoretical concern. Building an optimal Huffman tree over a real 2.7 MB corpus of source code produced a **maximum code length of 19 bits**, measured. Any implementation that assumes lengths fit in 15 bits without enforcing it will corrupt real files.

Maximum code length is driven by how skewed the frequency distribution is; a Fibonacci-like distribution forces depth to grow. With ~1M symbols per block, exceeding 15 bits is routine, not adversarial.

Two acceptable remedies:

The **package-merge algorithm** produces a provably optimal length-limited code. It is the correct answer and costs O(n·L) time.

The **zlib flatten heuristic** repeatedly moves depth from the deepest node to the shallowest available slot until all lengths are ≤ 15. It is simpler, near-optimal in practice (typically within 0.1% of optimal), and is what production zlib actually ships.

Recommendation: implement the heuristic in Phase 1 for simplicity, and cover it with a test that asserts the resulting lengths still satisfy Kraft equality and still round-trip. Revisit package-merge only if measurement shows the heuristic costs real ratio.

### 6.2 Canonical code construction

Codes are derived from lengths alone and never stored. Both encoder and decoder run the identical procedure (this is RFC 1951 §3.2.2, and it is worth following exactly rather than inventing a variant):

```
1. bl_count[len] = number of symbols having that code length, for len in 1..15
2. code = 0; bl_count[0] = 0
   for bits in 1..15:
       code = (code + bl_count[bits - 1]) << 1
       next_code[bits] = code
3. for sym in 0..255 in ascending order:
       if code_lengths[sym] != 0:
           codes[sym] = next_code[code_lengths[sym]]++
```

Because assignment walks symbols in ascending order, the mapping is fully determined by the length table. Two independent implementations agreeing on lengths will agree on codes.

### 6.3 Decoder validation

Before decoding a single symbol, the decoder MUST verify the length table describes a **complete** prefix code:

```
Σ over symbols with len > 0 of 2^(-len)  ==  1   exactly
```

Less than 1 means an incomplete code — reject (the single-symbol case is handled by RLE, §5.2, so it cannot legitimately appear here). Greater than 1 means an over-subscribed, non-prefix-free code — reject. Use integer arithmetic for this check, accumulating `1 << (15 - len)` and comparing against `1 << 15`, rather than floating point.

The decoder must also stop after emitting exactly `raw_len` symbols and MUST NOT trust the payload to self-terminate.

### 6.4 Table-driven decoding

Walking the tree one bit per node is O(code length) per symbol and cache-hostile. Because codes are canonical and capped at 15 bits, the decoder can instead build a single 32768-entry lookup table mapping the next 15 peeked bits directly to `(symbol, length)`, decoding in O(1) with one memory access.

At 4 bytes per entry that is a 128 KiB table, which is too large to rebuild per block cheaply. The standard compromise is a two-level table: a 9-bit primary table (512 entries) covering the common short codes, with overflow sub-tables for the rare long ones. Defer this to Phase 2 as a measured optimisation — correctness first.

---

## 7. LZ77_HUFFMAN block payload (v2)

Zyphie adopts **RFC 1951's length and distance code tables verbatim**. Those tables are proven, precisely documented, and mean the implementation can be differentially tested against zlib. Only the block framing and the code-length header (§6) are Zyphie's own, and both are simpler than DEFLATE's equivalents.

Parameters: sliding window 32 KiB, minimum match 3 bytes, maximum match 258 bytes — all matching DEFLATE so the distance and length code tables apply unchanged.

Two alphabets, each preceded by its own 4-bit-per-symbol length table:

**LITLEN**, 286 symbols (143 bytes of length table): `0–255` emit that literal byte; `256` marks end of block; `257–285` are match-length codes, some followed by extra raw bits per RFC 1951 Table 3.2.5.

**DIST**, 30 symbols (15 bytes of length table): match-distance codes, some followed by extra raw bits per RFC 1951 Table 3.2.5.

Decode loop: read a LITLEN symbol; if < 256 append the literal; if == 256 stop; otherwise resolve the length, read a DIST symbol, resolve the distance, and copy `length` bytes from `distance` back in the output.

The copy MUST tolerate overlap — `distance < length` is legal and is how runs are encoded (distance 1, length 100 means "repeat the previous byte 100 times"). This requires a forward byte-by-byte copy loop; `memcpy` is undefined behaviour here and `memmove` produces the wrong answer. This single line is the most commonly botched part of any LZ77 decoder.

Decoder MUST reject `distance == 0`, `distance >` bytes decoded so far in this block, and any length that would carry output past `raw_len`.

---

## 8. Block independence and the boundary cost

Matches never cross block boundaries. The first 32 KiB of each block therefore has a truncated view backwards, costing a little ratio.

At the default 1 MiB block, that is 32 KiB of 1 MiB — about 3% of the block operating with a partial window, which translates to well under 1% of overall ratio in practice. In exchange the format gets parallel compression across cores, parallel decompression, per-block resumability, and independent corruption blast radius.

This is the same trade bzip2 makes with its 900 KB independent blocks, and the opposite of gzip's single continuous stream. For a service that wants to use every available core on large files, independence is worth more than the fraction of a percent.

---

## 9. End of stream and footer

The terminator is a single `0xFF` byte with no `raw_len`/`enc_len` following it.

Footer, 12 bytes, present when `flags` bit 0 is set (it should always be set; the flag exists only for benchmarking the checksum's cost):

| Offset | Size | Field |
|---|---|---|
| 0 | 4 | `crc32` of the entire original uncompressed stream |
| 4 | 8 | `total_raw_len`, exact uncompressed length |

CRC-32 uses the standard IEEE 802.3 polynomial `0xEDB88320`, reflected, init `0xFFFFFFFF`, final XOR `0xFFFFFFFF` — identical to gzip and zlib, so results can be cross-checked against `gzip`'s output during development.

The decoder MUST compute the CRC over its own output and compare. On mismatch it MUST exit non-zero and MUST NOT present the output as valid. A compressor that can silently corrupt data has negative value; the checksum is what converts an unnoticed disaster into a clean, loud failure.

`total_raw_len` in the footer is authoritative over the header's `original_size`. When the header carries the unknown-sentinel this is the only place the true length appears.

---

## 10. Versioning policy

`format_version` is bumped only when a decoder that predates the change would misread a new file. Adding a new `block_type` is such a change, because an old decoder would reject the unknown type — which is the desired loud failure, but still requires a version bump so the failure message can be accurate.

The compatibility contract, which CI must enforce: **a v2 decoder decodes every v1 file.** The concrete mechanism is a small set of v1 files checked into the repository as golden fixtures, with a test that decodes them and byte-compares against their known-good originals. Those fixtures must never be regenerated — regenerating them is how projects silently break backward compatibility while all tests stay green.

---

## 11. Overhead summary

| Component | Cost | At 1 MiB block |
|---|---|---|
| Stream header | 16 B once | 0.0015% |
| Stream footer | 12 B once | 0.0011% |
| End-of-stream marker | 1 B once | 0.0001% |
| Block frame | 9 B per block | 0.0009% |
| v1 code-length table | 128 B per block | 0.0122% |
| v2 code-length tables | 158 B per block | 0.0151% |

Total metadata overhead is roughly **0.016%** of input at the default block size. Header compression would reduce this to about 0.010%. That difference is not worth the complexity, which is precisely why §6 specifies the flat table.

---

## 12. Reference test vectors

These MUST be checked into the repository as golden fixtures with their expected byte-exact compressed output, and they double as the edge-case corpus.

| Input | Expected structure |
|---|---|
| Empty (0 bytes) | header + `0xFF` + footer = 29 bytes, CRC of empty = `0x00000000` |
| Single byte `0x41` | one RLE block, `raw_len` 1 |
| 1 MiB of `0x00` | one RLE block, ~10 bytes payload |
| All 256 byte values once | one HUFFMAN block, all lengths equal 8, output ≈ input + overhead → likely STORED |
| 1 MiB `/dev/urandom` (fixed seed) | one STORED block |
| Exactly `1 << block_size_log` bytes | exactly one block, then terminator — **not** an empty second block |
| That size plus 1 byte | two blocks, second has `raw_len` 1 |
| That size minus 1 byte | one block |
| Text with a byte of frequency 1 among millions | exercises the 15-bit length limiter |

The three block-boundary cases matter disproportionately. Off-by-one errors at the final partial block are the single most common defect in blocked codecs, and they are invisible on typical test files because typical files do not land on a boundary.
