"""Independent V1 fixture encoder. Python is test tooling, never a product backend."""
import hashlib
import pathlib
import struct

ROOT = pathlib.Path(__file__).parent
digest = lambda b: hashlib.sha256(b).digest()
HEADER = struct.pack('<8sHHIIHHII', b'ZYPH\r\n\x1a\n', 1, 0, 32, 0, 1, 0, 4194304, 0)


def archive(name, kind=0, path='', data=b'', encoded=None, codec=0, mtime=None):
    payload = data if encoded is None else encoded
    block_count = int(bool(data))
    entries = b''
    if kind:
        p = path.encode('utf-8')
        entries = struct.pack('<BBHIQqQQ32s', kind, int(mtime is not None), 0, len(p), len(data), mtime or 0,
                              0, block_count, digest(data) if kind == 1 else bytes(32)) + p
    manifest = b'ZYPHMAN1' + struct.pack('<Q', int(bool(kind))) + entries
    index = b'ZYPHIDX1' + struct.pack('<Q', block_count)
    if block_count:
        index += struct.pack('<QIIBBH32sI', 32, len(payload), len(data), codec, 0, 0, digest(data), 0)
    mo = 32 + len(payload)
    io = mo + len(manifest)
    total = io + len(index) + 128
    footer = struct.pack('<8sQQQQQ16s32s', b'ZYPHEND1', mo, len(manifest), io,
                         len(index), total, bytes(16), digest(HEADER + manifest + index))
    result = HEADER + payload + manifest + index + footer + digest(footer)
    assert len(result) == total
    (ROOT / name).write_bytes(result)
    return f'{hashlib.sha256(result).hexdigest()}  {name}'


if __name__ == '__main__':
    checksums = [archive('empty.zyph'), archive('empty-file.zyph', 1, 'empty.txt'),
                 archive('empty-directory.zyph', 2, 'empty-dir'),
                 archive('store-abc.zyph', 1, 'abc.txt', b'abc'),
                 archive('store-timestamp.zyph', 1, 'abc.txt', b'abc', mtime=1700000000),
                 # Hand-encoded Zstandard single-segment frame, content size 64,
                 # one last RLE block: 64 copies of ASCII 'a'. No codec dependency.
                 archive('zstd-rle.zyph', 1, 'repeat.txt', b'a' * 64,
                         bytes.fromhex('28b52ffd204003020061'), 1)]
    (ROOT / 'SHA256SUMS').write_text('\n'.join(checksums) + '\n', encoding='ascii')
