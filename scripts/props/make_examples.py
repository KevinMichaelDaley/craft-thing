"""Generate deterministic editable RGBA/indexed LibreSprite template/examples."""
import json
from pathlib import Path
import struct
import zlib

ROOT = Path(__file__).resolve().parents[2]
SCHEMA = json.loads((ROOT / 'assets/props/schema-v1.json').read_text())
WIDTH, HEIGHT = 48, 32


def chunk(kind, payload):
    return struct.pack('<IH', len(payload) + 6, kind) + payload


def string(value):
    value = value.encode()
    return struct.pack('<H', len(value)) + value


def document(empty=False):
    channels = {name: bytearray(WIDTH * HEIGHT * 4) for name in
                ('physics.material', 'physics.role', 'physics.object', 'physics.anchor', 'color', 'emissive')}
    materials = {m['id']: m for m in SCHEMA['materials']}
    roles = {r['id']: r for r in SCHEMA['roles']}

    def pixel(x, y, material, role, obj=0):
        at = (y * WIDTH + x) * 4
        channels['physics.material'][at:at + 4] = bytes(materials[material]['rgba'])
        channels['physics.role'][at:at + 4] = bytes(roles[role]['rgba'])
        if obj:
            channels['physics.object'][at:at + 4] = obj.to_bytes(3, 'little') + b'\xff'
        grain = (x * 7 + y * 3) % 19
        channels['color'][at:at + 4] = bytes((90 + grain, 55 + grain, 30 + grain, 255))

    if not empty:
        for y in (30, 31):
            for x in range(WIDTH):
                pixel(x, y, 1, 1)
        for y in range(17, 30):
            for x in (4, 5):
                pixel(x, y, 2, 2, 1)
        for x in range(4, 21):
            pixel(x, 16, 2, 2, 1)
        for x in (4, 5):
            at = (29 * WIDTH + x) * 4
            channels['physics.anchor'][at:at + 4] = b'\xff' * 4
        for y in range(3, 12):
            for x in range(24, 34):
                if x in (24, 25, 32, 33) or y >= 10:
                    pixel(x, y, 1, 3, 2)
        for y in range(14, 25):
            for x in range(36, 44):
                if x <= 37 or y in (14, 15, 19, 20, 23, 24):
                    pixel(x, y, 2 if x <= 37 else 4, 3, 3)
        at = (5 * WIDTH + 5) * 4
        channels['physics.role'][at:at + 4] = bytes(roles[4]['rgba'])
        channels['emissive'][at:at + 4] = bytes((255, 180, 80, 255))
    frames = []
    for index, duration in enumerate((90, 170) if not empty else (90,)):
        frame_channels = {name: bytes(value) for name, value in channels.items()}
        if index:
            emission = bytearray(frame_channels['emissive'])
            emission[(5 * WIDTH + 5) * 4:(5 * WIDTH + 5) * 4 + 4] = bytes((80, 140, 255, 255))
            frame_channels['emissive'] = bytes(emission)
        frames.append({'duration_ms': duration, 'channels': frame_channels})
    return {'canvas': [WIDTH, HEIGHT], 'frames': frames, 'tags': [] if empty else [
        {'name': 'idle', 'from': 0, 'to': 1, 'direction': 'forward', 'repeat': 0}]}


def ase(document, indexed=False):
    names = list(document['frames'][0]['channels'])
    palette = [(0, 0, 0, 0)]
    if indexed:
        for frame in document['frames']:
            for data in frame['channels'].values():
                for at in range(0, len(data), 4):
                    color = tuple(data[at:at + 4])
                    if color not in palette:
                        palette.append(color)
        if len(palette) > 256:
            raise ValueError('Example exceeds indexed palette')
    frames = []
    for index, frame in enumerate(document['frames']):
        chunks = []
        if not index:
            if indexed:
                entries = b''.join(struct.pack('<H4B', 0, *color) for color in palette)
                chunks.append(chunk(0x2019, struct.pack('<3I', len(palette), 0, len(palette) - 1) + bytes(8) + entries))
            for name in names:
                flags = 2 if name.startswith('physics.') else 3
                chunks.append(chunk(0x2004, struct.pack('<6HB', flags, 0, 0, 0, 0, 0, 255) + bytes(3) + string(name)))
            if document['tags']:
                tags = []
                for tag in document['tags']:
                    tags.append(struct.pack('<HHBH', tag['from'], tag['to'], 0, tag['repeat'])
                                + bytes(6) + bytes((255, 255, 255, 0)) + string(tag['name']))
                chunks.append(chunk(0x2018, struct.pack('<H', len(tags)) + bytes(8) + b''.join(tags)))
        for layer, name in enumerate(names):
            linked = index and name.startswith('physics.')
            cel = struct.pack('<HhhBH', layer, 0, 0, 255, 1 if linked else 2) + bytes(7)
            if linked:
                cel += struct.pack('<H', 0)
            else:
                raw = frame['channels'][name]
                if indexed:
                    raw = bytes(palette.index(tuple(raw[at:at + 4])) for at in range(0, len(raw), 4))
                cel += struct.pack('<HH', *document['canvas']) + zlib.compress(raw)
            chunks.append(chunk(0x2005, cel))
        body = b''.join(chunks)
        frames.append(struct.pack('<IHHH', 16 + len(body), 0xf1fa, len(chunks), frame['duration_ms']) + bytes(6) + body)
    header = bytearray(128)
    struct.pack_into('<I5H', header, 0, 128 + sum(map(len, frames)), 0xa5e0, len(frames), *document['canvas'], 8 if indexed else 32)
    struct.pack_into('<I', header, 14, 1)
    struct.pack_into('<H', header, 18, 90)
    header[32] = header[33] = 1
    return bytes(header) + b''.join(frames)


if __name__ == '__main__':
    destination = ROOT / 'assets/props'
    for name, empty, indexed in (('template', True, False), ('example-rgba', False, False), ('example-indexed', False, True)):
        (destination / (name + '.aseprite')).write_bytes(ase(document(empty), indexed))
    for name, legend in (('material', SCHEMA['materials']), ('role', SCHEMA['roles'])):
        lines = ['GIMP Palette', 'Name: Craft ' + name, 'Columns: 8', '# Transparent = absent; metadata labels must be opaque']
        lines += [f'{item["rgba"][0]:3} {item["rgba"][1]:3} {item["rgba"][2]:3} {item["name"]}' for item in legend]
        (destination / (name + '.gpl')).write_text('\n'.join(lines) + '\n')
