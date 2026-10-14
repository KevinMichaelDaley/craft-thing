"""Small deterministic ASE documents for real-host compatibility tests."""
import struct
import zlib


def chunk(kind, payload):
    return struct.pack('<IH', len(payload) + 6, kind) + payload


def fixture(path):
    frames = []
    for frame, duration in enumerate((90, 170)):
        chunks = []
        if frame == 0:
            for name in ('physics.material', 'physics.role', 'color'):
                encoded = name.encode()
                flags = 2 if name.startswith('physics.') else 3
                chunks.append(chunk(0x2004, struct.pack('<6HB', flags, 0, 0, 0, 0, 0, 255)
                                    + bytes(3) + struct.pack('<H', len(encoded)) + encoded))
        for layer in range(3):
            base = struct.pack('<HhhBH', layer, 1, 2, 255, 1 if frame and layer < 2 else 2) + bytes(7)
            if frame and layer < 2:
                data = struct.pack('<H', 0)
            else:
                rgba = ((96, 96, 96, 255), (0, 0, 255, 255), (80 + frame * 30, 30, 10, 255))[layer]
                data = struct.pack('<HH', 2, 1) + zlib.compress(bytes(rgba) + bytes(4))
            chunks.append(chunk(0x2005, base + data))
        body = b''.join(chunks)
        frames.append(struct.pack('<IHHH', len(body) + 16, 0xf1fa, len(chunks), duration) + bytes(6) + body)
    header = bytearray(128)
    struct.pack_into('<I6HI', header, 0, 128 + sum(map(len, frames)), 0xa5e0, 2, 5, 5, 32, 1, 100)
    header[34] = 0
    header[42] = header[43] = 1
    path.write_bytes(bytes(header) + b''.join(frames))
