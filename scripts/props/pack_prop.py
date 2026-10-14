"""Validate and atomically publish lossless LibreSprite prop packages (offline)."""
import ctypes
import hashlib
import json
import math
import os
from pathlib import Path
import shutil
import struct
import tempfile
import uuid
import zlib

from ase_reader import MAX_FRAMES, MAX_PIXELS, read

ROOT = Path(__file__).resolve().parents[2]
SCHEMA = json.loads((ROOT / 'assets/props/schema-v1.json').read_text())
MATERIALS = {m['id']: m for m in SCHEMA['materials']}
MATERIAL_COLORS = {bytes(m['rgba']): m['id'] for m in SCHEMA['materials']}
ROLE_COLORS = {bytes(r['rgba']): r['id'] for r in SCHEMA['roles']}
VISUAL = ('color', 'emissive', 'normal', 'roughness', 'relief')
MAX_COMPILED_BYTES = 256 * 1024 * 1024
DEFAULTS = {'physics_animation': 'visual_only', 'pivot': [0, 0],
            'physical_scale': [1, 1], 'visual_scale': [1, 1],
            'emission_multiplier': 1, 'relief_scale': 1, 'allow_invisible_physics': False}


def canonical(value):
    return (json.dumps(value, sort_keys=True, separators=(',', ':'), allow_nan=False) + '\n').encode()


def label(rgba, legend, location):
    if rgba[3] == 0:
        return 0
    if rgba not in legend:
        raise ValueError(f'{location}: unknown or partial-alpha metadata label {list(rgba)}')
    return legend[rgba]


def png(width, height, rgba):
    def chunk(kind, payload):
        return struct.pack('>I', len(payload)) + kind + payload + struct.pack('>I', zlib.crc32(kind + payload))
    rows = b''.join(b'\0' + rgba[y * width * 4:(y + 1) * width * 4] for y in range(height))
    return b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>2I5B', width, height, 8, 6, 0, 0, 0)) + chunk(b'IDAT', zlib.compress(rows, 9)) + chunk(b'IEND', b'')


def decode_png(data, width, height):
    if data[:8] != b'\x89PNG\r\n\x1a\n':
        raise ValueError('Invalid PNG signature')
    position, packed, kinds = 8, b'', []
    while position < len(data):
        if position + 12 > len(data):
            raise ValueError('Truncated PNG')
        size, = struct.unpack_from('>I', data, position)
        kind = data[position + 4:position + 8]
        payload = data[position + 8:position + 8 + size]
        if position + size + 12 > len(data) or zlib.crc32(kind + payload) != struct.unpack_from('>I', data, position + 8 + size)[0]:
            raise ValueError('Invalid PNG length/CRC')
        if kind == b'IHDR' and payload != struct.pack('>2I5B', width, height, 8, 6, 0, 0, 0):
            raise ValueError('PNG channel alignment/format mismatch')
        if kind == b'IDAT':
            packed += payload
        kinds.append(kind)
        position += size + 12
    if kinds != [b'IHDR', b'IDAT', b'IEND']:
        raise ValueError('Unsupported PNG chunk layout')
    stream = zlib.decompressobj()
    size = height * (width * 4 + 1)
    raw = stream.decompress(packed, size + 1)
    if len(raw) != size or not stream.eof or stream.unused_data or stream.unconsumed_tail:
        raise ValueError('PNG decoded length mismatch')
    if any(raw[y * (width * 4 + 1)] for y in range(height)):
        raise ValueError('Unsupported PNG filter')
    return b''.join(raw[y * (width * 4 + 1) + 1:(y + 1) * (width * 4 + 1)] for y in range(height))


def linear(channel):
    value = channel / 255
    return value / 12.92 if value <= .04045 else ((value + .055) / 1.055) ** 2.4


def checked_options(options):
    options = {**DEFAULTS, **(options or {})}
    if options['physics_animation'] not in ('visual_only', 'state_variants'):
        raise ValueError('Unsupported physics animation policy')
    for key in ('physical_scale', 'visual_scale'):
        if options[key] != [1, 1]:
            raise ValueError('v1 supports one-to-one pixels only; no metadata resampling or implicit high-resolution mapping')
    if not isinstance(options['allow_invisible_physics'], bool):
        raise ValueError('Invisible physics flag must be boolean')
    if len(options['pivot']) != 2 or not all(type(v) is int and -2147483648 <= v <= 2147483647 for v in options['pivot']):
        raise ValueError('Pivot must be two bounded integer cell coordinates')
    for key in ('emission_multiplier', 'relief_scale'):
        if not isinstance(options[key], (int, float)) or not math.isfinite(options[key]) or not 0 <= options[key] <= 1e6:
            raise ValueError('Invalid emission/relief units')
    return options


def physical(channels, count, frame, options):
    if not all(name in channels for name in ('physics.material', 'physics.role', 'color')):
        raise ValueError('Required material, role and color channels missing')
    records = bytearray()
    objects = channels.get('physics.object', bytes(count * 4))
    anchors = channels.get('physics.anchor', bytes(count * 4))
    for pixel in range(count):
        at = pixel * 4
        location = f'frame {frame} pixel {pixel}'
        material = label(channels['physics.material'][at:at + 4], MATERIAL_COLORS, location)
        role = label(channels['physics.role'][at:at + 4], ROLE_COLORS, location)
        if material and (role not in (1, 2, 3) or MATERIALS[material]['terrain' if role == 1 else 'body'] is None):
            raise ValueError(location + ': incompatible material/role')
        if not material and role in (1, 2, 3):
            raise ValueError(location + ': physical role has no material')
        if material and not channels['color'][at + 3] and not options['allow_invisible_physics']:
            raise ValueError(location + ': invisible physics requires explicit authoring flag')
        obj = objects[at:at + 4]
        if obj[3] not in (0, 255):
            raise ValueError(location + ': partial-alpha object ID')
        object_id = int.from_bytes(obj[:3], 'little') if obj[3] else 0
        anchor = anchors[at:at + 4]
        anchor = label(anchor, {b'\xff\xff\xff\xff': 1}, location)
        if anchor and role != 2:
            raise ValueError(location + ': only structures may carry anchor hints')
        records.extend(struct.pack('<HBBI', material, role, anchor, object_id))
    return bytes(records)


def appearance(channels, count, options):
    records = bytearray()
    for pixel in range(count):
        at = pixel * 4
        color = channels['color'][at:at + 4]
        emit = channels.get('emissive', b'')[at:at + 4] or bytes(4)
        normal = channels.get('normal', b'')[at:at + 4]
        n = [0., 0., 1.] if not normal or not normal[3] else [2 * v / 255 - 1 for v in normal[:3]]
        length = math.sqrt(sum(v * v for v in n))
        if length < 1e-8:
            raise ValueError('Degenerate normal')
        n = [v / length for v in n]
        rough = channels.get('roughness', b'')[at:at + 4]
        relief = channels.get('relief', b'')[at:at + 4]
        for scalar in (rough, relief):
            if scalar and scalar[3] and (scalar[0] != scalar[1] or scalar[1] != scalar[2]):
                raise ValueError('Scalar roughness/relief must be grayscale')
        roughness = rough[0] / 255 if rough and rough[3] else .8
        height = relief[0] / 255 * options['relief_scale'] if relief and relief[3] else 0
        emission = [linear(v) * emit[3] / 255 * options['emission_multiplier'] for v in emit[:3]]
        surface_mask = (1 if normal and normal[3] else 0) | (2 if rough and rough[3] else 0) | (4 if relief and relief[3] else 0)
        # First32bytes are the renderer appearance ABI; independent emission/alpha follow.
        records.extend(struct.pack('<15fI', *[linear(v) for v in color[:3]], roughness,
                                   *n, height, *emission, 0., color[3] / 255, 0., 0., surface_mask))
    return bytes(records)


def build(document, options=None):
    options = checked_options(options)
    width, height = document['canvas']
    if type(width) is not int or type(height) is not int or not width or not height or width < 0 or height < 0 or width * height > MAX_PIXELS:
        raise ValueError('Invalid canvas dimensions')
    frames = document['frames']
    if not 0 < len(frames) <= MAX_FRAMES or width * height * len(frames) > MAX_PIXELS * 16:
        raise ValueError('Frame/pixel budget exceeded')
    if 64 + len(frames) * (16 + width * height * 72) > MAX_COMPILED_BYTES:
        raise ValueError('Compiled asset exceeds256MiB preload budget; split scene into packages')
    payloads, descriptors, physics = {}, [], []
    count = width * height
    for index, frame in enumerate(frames):
        channels = frame['channels']
        if any(name not in SCHEMA['channels'] or len(value) != count * 4 for name, value in channels.items()):
            raise ValueError('Channel name/decoded alignment mismatch')
        if 'markers' in channels and any(channels['markers'][3::4]):
            raise ValueError('v1 marker import unsupported: use manifest pivot, do not silently discard annotations')
        duration = frame['duration_ms']
        if type(duration) is not int or not 0 < duration <= 65535:
            raise ValueError('Frame duration must be integer milliseconds in1..65535')
        maps = physical(channels, count, index, options)
        if physics and maps != physics[0] and options['physics_animation'] == 'visual_only':
            pixel = next(p for p in range(count) if maps[p * 8:p * 8 + 8] != physics[0][p * 8:p * 8 + 8])
            raise ValueError(f'frame {index} pixel {pixel}: physical metadata changed under visual_only')
        physics.append(maps)
        paths = {}
        for name in VISUAL:
            if name in channels:
                path = f'{index:04}.{name}.png'
                payloads[path] = png(width, height, channels[name])
                paths[name] = path
        for name, size, offset in (('material', 2, 0), ('role', 1, 2), ('anchor', 1, 3), ('object', 4, 4)):
            path = f'{index:04}.{name}.u{size * 8}'
            payloads[path] = b''.join(maps[p * 8 + offset:p * 8 + offset + size] for p in range(count))
            paths[name] = path
        descriptors.append({'duration_ms': duration, 'channels': paths})
    tags = document.get('tags', [])
    names = set()
    for tag in tags:
        if not isinstance(tag['name'], str) or tag['name'] in names or not 0 <= tag['from'] <= tag['to'] < len(frames) or tag['direction'] not in ('forward', 'reverse', 'pingpong', 'pingpong_reverse') or not 0 <= tag['repeat'] <= 65535:
            raise ValueError('Invalid/duplicate animation tag')
        names.add(tag['name'])
    identity = struct.pack('<II', width, height) + physics[0]
    asset_uuid = str(uuid.UUID(options['asset_uuid'])) if 'asset_uuid' in options else str(uuid.uuid5(uuid.NAMESPACE_URL, hashlib.sha256(identity).hexdigest()))
    manifest = {'schema': 'craft.prop', 'version': 1, 'registry': SCHEMA['registry'],
                'asset_uuid': asset_uuid, 'canvas': [width, height], 'options': options,
                'materials': SCHEMA['materials'], 'roles': SCHEMA['roles'],
                'frames': descriptors, 'tags': tags, 'source_mode': document.get('mode', 'rgba'),
                'exporter': 'craft-libresprite-1', 'host': 'LibreSprite-1.3'}
    frame_table = bytearray()
    body = bytearray()
    for index, frame in enumerate(frames):
        frame_table.extend(struct.pack('<4I', frame['duration_ms'], len(body), count * 8, count * 64))
        body.extend(physics[index])
        body.extend(appearance(frame['channels'], count, options))
    compiled_payload = bytes(frame_table + body)
    header = struct.pack('<4s11I16s', b'CPRP', 1, 64 + len(compiled_payload), width, height, len(frames),
                         1 if options['physics_animation'] == 'state_variants' else 0,
                         64, 64 + len(frame_table), 8, 64, 0, uuid.UUID(asset_uuid).bytes)
    checksum = zlib.crc32(header[:44] + header[48:] + compiled_payload)
    header = header[:44] + struct.pack('<I', checksum) + header[48:]
    payloads['asset.cprop'] = header + compiled_payload
    manifest['files'] = {name: hashlib.sha256(value).hexdigest() for name, value in sorted(payloads.items())}
    payloads['manifest.json'] = canonical(manifest)
    return payloads


def validate(directory):
    directory = Path(directory).resolve()
    manifest = json.loads((directory / 'manifest.json').read_text())
    if manifest.get('schema') != 'craft.prop' or manifest.get('version') != 1 or manifest.get('registry') != SCHEMA['registry']:
        raise ValueError('Unsupported prop schema/registry')
    if manifest.get('materials') != SCHEMA['materials'] or manifest.get('roles') != SCHEMA['roles']:
        raise ValueError('Canonical material/role registry mismatch')
    width, height = manifest['canvas']
    options = checked_options(manifest['options'])
    checked = {}
    for name, digest in manifest['files'].items():
        path = Path(name)
        if path.is_absolute() or '..' in path.parts or path.name != name or (directory / path).is_symlink():
            raise ValueError('Unsafe package path')
        value = (directory / path).read_bytes()
        if hashlib.sha256(value).hexdigest() != digest:
            raise ValueError('Package file integrity mismatch: ' + name)
        checked[name] = value
    document = {'canvas': [width, height], 'frames': [], 'tags': manifest['tags'], 'mode': manifest['source_mode']}
    for frame in manifest['frames']:
        channels, maps = {}, frame['channels']
        for name in VISUAL:
            if name in maps:
                channels[name] = decode_png(checked[maps[name]], width, height)
        materials = checked[maps['material']]
        roles = checked[maps['role']]
        objects = checked[maps['object']]
        anchors = checked[maps['anchor']]
        if [len(materials), len(roles), len(objects), len(anchors)] != [width * height * 2, width * height, width * height * 4, width * height]:
            raise ValueError('Typed map decoded length mismatch')
        material_rgba, role_rgba, object_rgba, anchor_rgba = bytearray(), bytearray(), bytearray(), bytearray()
        role_defs = {r['id']: r for r in SCHEMA['roles']}
        for pixel in range(width * height):
            material, = struct.unpack_from('<H', materials, pixel * 2)
            role = roles[pixel]
            if material not in MATERIALS and material or role not in role_defs and role:
                raise ValueError('Unknown typed map code')
            material_rgba.extend(bytes(MATERIALS[material]['rgba']) if material else bytes(4))
            role_rgba.extend(bytes(role_defs[role]['rgba']) if role else bytes(4))
            obj, = struct.unpack_from('<I', objects, pixel * 4)
            if obj > 0xffffff or anchors[pixel] > 1:
                raise ValueError('Object/anchor code out of range')
            object_rgba.extend(obj.to_bytes(3, 'little') + bytes((255 if obj else 0,)))
            anchor_rgba.extend(bytes((255,)) * 4 if anchors[pixel] else bytes(4))
        channels.update({'physics.material': bytes(material_rgba), 'physics.role': bytes(role_rgba),
                         'physics.object': bytes(object_rgba), 'physics.anchor': bytes(anchor_rgba)})
        document['frames'].append({'duration_ms': frame['duration_ms'], 'channels': channels})
    regenerated = build(document, {**options, 'asset_uuid': manifest['asset_uuid']})
    if regenerated['asset.cprop'] != checked['asset.cprop']:
        raise ValueError('Compiled asset disagrees with package channels')
    return manifest


def export(document, output, options=None):
    payloads = build(document, options)
    output = Path(output).absolute()
    output.parent.mkdir(parents=True, exist_ok=True)
    stage = Path(tempfile.mkdtemp(prefix='.' + output.name + '.', dir=output.parent))
    try:
        for name, value in payloads.items():
            (stage / name).write_bytes(value)
        validate(stage)
        if output.exists():
            if output.is_symlink() or not output.is_dir():
                raise ValueError('Existing output must be a real package directory')
            libc = ctypes.CDLL(None, use_errno=True)
            rename = getattr(libc, 'renameat2', None)
            if rename is None or rename(-100, os.fsencode(stage), -100, os.fsencode(output), 2):
                raise OSError(ctypes.get_errno(), 'Atomic directory exchange unavailable; previous package preserved')
        else:
            os.replace(stage, output)
    finally:
        if stage.exists():
            shutil.rmtree(stage)


def read_request_snapshot(request):
    document = read(request['snapshot'])
    proof = request.get('proof')
    actual = document['proof']
    order = lambda cel: (cel['layer'], cel['frame'])
    if not proof or proof.get('canvas') != actual['canvas'] or sorted(proof.get('cels', []), key=order) != sorted(actual['cels'], key=order):
        raise ValueError('Active snapshot verification failed; host save did not capture current pixels')
    return document


if __name__ == '__main__':
    import argparse
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--request', type=Path)
    parser.add_argument('--snapshot', type=Path)
    parser.add_argument('--output', type=Path)
    parser.add_argument('--validate', type=Path)
    args = parser.parse_args()
    if args.validate:
        validate(args.validate)
    else:
        if args.request:
            request = json.loads(args.request.read_text())
            if request.get('schema') != 'craft.prop-export-request' or request.get('version') != 1:
                parser.error('Unsupported export request')
            snapshot, options = Path(request['snapshot']), request['options']
            # Compatibility guard only: pixels/timing always come from active snapshot.
            if request.get('source') and Path(request['source']).is_file():
                original = Path(request['source']).read_bytes()
                if len(original) >= 14 and original[4:6] == b'\xe0\xa5' and original[12:14] == b'\x08\x00':
                    read(request['source'])
        elif args.snapshot:
            snapshot, options = args.snapshot, None
        else:
            parser.error('--request or --snapshot required')
        if not args.output:
            parser.error('--output required')
        document = read_request_snapshot(request) if args.request else read(snapshot)
        export(document, args.output, options)
        print('Validated prop package:', args.output)
