"""Strict lossless ASE snapshot reader for the pinned LibreSprite adapter."""
from pathlib import Path
import struct
import zlib

MAX_PIXELS = 4_194_304
MAX_FRAMES = 4096
CHANNELS = {'physics.material', 'physics.role', 'physics.object', 'physics.anchor',
            'color', 'emissive', 'normal', 'roughness', 'relief', 'markers'}


def unpack(fmt, data, offset=0):
    try:
        return struct.unpack_from('<' + fmt, data, offset)
    except struct.error as error:
        raise ValueError('Truncated ASE data') from error


def text(data, offset):
    length, = unpack('H', data, offset)
    raw = data[offset + 2:offset + 2 + length]
    if len(raw) != length:
        raise ValueError('Truncated ASE string')
    return raw.decode('utf-8')


def decompress(data, size):
    stream = zlib.decompressobj()
    raw = stream.decompress(data, size + 1)
    if len(raw) != size or not stream.eof or stream.unused_data or stream.unconsumed_tail:
        raise ValueError('Invalid or oversized compressed cel')
    return raw


def read(path):
    data = Path(path).read_bytes()
    length, magic, count, width, height, depth = unpack('I5H', data)
    if length != len(data) or magic != 0xa5e0 or len(data) < 128:
        raise ValueError('Invalid ASE header/length')
    if not width or not height or width * height > MAX_PIXELS or not 0 < count <= MAX_FRAMES:
        raise ValueError('ASE dimensions/frame count exceed limits')
    if depth not in (8, 32):
        raise ValueError('Only RGBA and indexed documents supported; convert grayscale explicitly')
    transparent = data[28]
    palette = [(0, 0, 0, 255)] * 256
    layers, frames, tags, history = [], [], [], {}
    cursor = 128
    for frame_index in range(count):
        frame_size, frame_magic, old_count, duration = unpack('I3H', data, cursor)
        new_count, = unpack('I', data, cursor + 12)
        if frame_magic != 0xf1fa or frame_size < 16 or cursor + frame_size > length or not duration:
            raise ValueError('Invalid ASE frame/duration')
        end = cursor + frame_size
        position = cursor + 16
        cels = []
        for _ in range(new_count or old_count):
            chunk_size, kind = unpack('IH', data, position)
            if chunk_size < 6 or position + chunk_size > end:
                raise ValueError('Invalid ASE chunk extent')
            payload = data[position + 6:position + chunk_size]
            if kind == 0x2004:
                flags, layer_type, child, _, _, blend = unpack('6H', payload)
                name = text(payload, 16)
                if layer_type not in (0, 1):
                    raise ValueError('Tilemap layer unsupported')
                layers.append({'name': name, 'type': layer_type, 'level': child,
                               'flags': flags, 'blend': blend, 'opacity': payload[12]})
            elif kind == 0x2019:
                palette_size, first, last = unpack('3I', payload)
                if not 0 < palette_size <= 256 or first > last or last >= palette_size:
                    raise ValueError('Invalid indexed palette range')
                at = 20
                for index in range(first, last + 1):
                    flags, r, g, b, a = unpack('H4B', payload, at)
                    palette[index] = (r, g, b, a)
                    at += 6
                    if flags & 1:
                        name = text(payload, at)
                        at += 2 + len(name.encode('utf-8'))
            elif kind in (0x0004, 0x0011):
                packets, = unpack('H', payload)
                at, index = 2, 0
                for _ in range(packets):
                    skip, amount = unpack('2B', payload, at)
                    index += skip
                    at += 2
                    for _ in range(amount or 256):
                        if index >= 256:
                            raise ValueError('Palette overflow')
                        rgb = unpack('3B', payload, at)
                        palette[index] = tuple(v * 255 // 63 if kind == 0x0011 else v for v in rgb) + (255,)
                        index += 1
                        at += 3
            elif kind == 0x2005:
                layer, x, y, opacity, cel_type = unpack('HhhBH', payload)
                if layer >= len(layers):
                    raise ValueError('Cel references unknown layer')
                if cel_type == 1:
                    linked, = unpack('H', payload, 16)
                    if linked >= frame_index or (layer, linked) not in history:
                        raise ValueError('Invalid linked cel frame')
                    cw, ch, pixels = history[layer, linked]
                elif cel_type in (0, 2):
                    cw, ch = unpack('HH', payload, 16)
                    if not cw or not ch or cw * ch > MAX_PIXELS:
                        raise ValueError('Invalid cel dimensions')
                    size = cw * ch * (depth // 8)
                    pixels = decompress(payload[20:], size) if cel_type == 2 else payload[20:]
                    if len(pixels) != size:
                        raise ValueError('Cel decoded length mismatch')
                else:
                    raise ValueError('Unsupported ASE cel type')
                history[layer, frame_index] = (cw, ch, pixels)
                cels.append((layer, x, y, opacity, cw, ch, pixels))
            elif kind == 0x2018:
                tag_count, = unpack('H', payload)
                at = 10
                for _ in range(tag_count):
                    first, last, direction = unpack('HHB', payload, at)
                    repeat, = unpack('H', payload, at + 5)
                    name = text(payload, at + 17)
                    if first > last or last >= count or direction > 3:
                        raise ValueError('Invalid animation tag bounds/direction')
                    tags.append({'name': name, 'from': first, 'to': last,
                                 'direction': ('forward', 'reverse', 'pingpong', 'pingpong_reverse')[direction],
                                 'repeat': repeat})
                    at += 19 + len(name.encode('utf-8'))
            position += chunk_size
        if position != end:
            raise ValueError('ASE frame chunk count/length mismatch')
        channels = {}
        names = set()
        for layer in layers:
            if layer['name'] in CHANNELS:
                if layer['name'] in names or layer['type'] != 0:
                    raise ValueError('Duplicate or non-image reserved layer: ' + layer['name'])
                names.add(layer['name'])
                channels[layer['name']] = bytearray(width * height * 4)
        seen = set()
        for layer_index, x, y, opacity, cw, ch, pixels in cels:
            layer = layers[layer_index]
            name = layer['name']
            if name not in channels:
                continue
            if name in seen:
                raise ValueError('Duplicate cel at layer/frame')
            seen.add(name)
            if layer['blend'] or opacity != 255 or layer['opacity'] != 255:
                raise ValueError('Export channels require normal blend and opacity 255: ' + name)
            for cy in range(ch):
                for cx in range(cw):
                    source = cy * cw + cx
                    rgba = pixels[4 * source:4 * source + 4] if depth == 32 else bytes(
                        (0, 0, 0, 0) if pixels[source] == transparent else palette[pixels[source]])
                    if 0 <= x + cx < width and 0 <= y + cy < height:
                        dest = ((y + cy) * width + x + cx) * 4
                        channels[name][dest:dest + 4] = rgba
                    elif rgba[3]:
                        raise ValueError('Occupied cel pixels outside shared canvas')
        frames.append({'duration_ms': duration, 'channels': {k: bytes(v) for k, v in channels.items()}})
        cursor = end
    if cursor != length:
        raise ValueError('Trailing ASE frames')
    return {'canvas': [width, height], 'frames': frames, 'tags': tags,
            'layers': layers, 'mode': 'rgba' if depth == 32 else 'indexed'}
