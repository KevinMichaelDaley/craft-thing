"""Lossless authored beaver import using the supported LibreSprite ASE reader.

The checked-in sidecar records explicit region and one-to-one parcel maps.
--author-maps initializes that sidecar; normal builds only validate/read it.
No runtime image parsing or mutable state is generated.
"""
import argparse
import hashlib
import json
from pathlib import Path

from ase_reader import read

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / 'assets/player/beaver/beaver_chomp.ase'
MAP = SOURCE.with_suffix('.mapping.json')
HEADER = ROOT / 'src/player/beaver_imported.h'
CAST = SOURCE.with_name('beaver_cast.ase')
CAST_MAP = CAST.with_suffix('.mapping.json')


def cast_maps(frames, canonical):
    canonical = list(canonical)
    regions, sources = [], []
    for f, rgba in enumerate(frames):
        region = [0] * 3200
        for p in range(3200):
            r, g, b, a = rgba[p*4:p*4+4]
            if not a:
                continue
            if (r, g, b) in ((83, 151, 153), (63, 120, 135)):
                region[p] = 3 if p//80 < (18, 17, 14, 14, 18, 22, 22, 22)[f] else 2
            elif (r, g, b) in ((78, 39, 35), (167, 103, 62), (161, 185, 167)):
                region[p] = 1
            elif (r, g, b) in ((113, 106, 83), (45, 40, 36)):
                region[p] = 4
            elif (r, g, b) == (187, 139, 76):
                region[p] = 6
            elif (r, g, b) != (0, 0, 0):
                raise ValueError(('Unassigned cast color', f, p, (r, g, b, a)))
        colored = [p for p, r in enumerate(region) if r]
        for p in range(3200):
            if rgba[p*4+3] and not region[p]:
                q = min(colored, key=lambda q: ((p%80-q%80)**2+(p//80-q//80)**2, q))
                region[p] = region[q]
        mapping = [0] * 3200
        for r in range(1, 7):
            destinations = {p for p, value in enumerate(region) if value == r}
            available = {p for p, value in enumerate(canonical) if value == r}
            pairs = sorted(((p%80-q%40)**2+(p//80-q//40)**2, p, q)
                           for p in destinations for q in available)
            for _, p, q in pairs:
                if p in destinations and q in available:
                    mapping[p] = q+1
                    destinations.remove(p)
                    available.remove(q)
            for p in sorted(destinations):
                q = min((q for q, value in enumerate(canonical) if not value),
                        key=lambda q: ((p%80-q%40)**2+(p//80-q//40)**2, q))
                canonical[q] = r
                mapping[p] = q+1
        regions.append(region)
        sources.append(mapping)
    return {'source_sha256': hashlib.sha256(CAST.read_bytes()).hexdigest(),
            'regions': regions, 'sources': sources, 'canonical_regions': canonical,
            'policy': 'Immutable chomp canonical IDs extended for cast. Cyan upper hat/lower robe; '
                      'brown fur, dark tail; ochre staff uses wood. Black outlines inherit nearest '
                      'explicit region. Unique same-region owners per frame, hidden owners retained.'}


def author_maps(frames):
    regions = []
    for f, rgba in enumerate(frames):
        pixels = [tuple(rgba[i:i+4]) for i in range(0, len(rgba), 4)]
        region = [0] * 1600
        for p, (r, g, b, a) in enumerate(pixels):
            if not a:
                continue
            x, y = p % 40, p // 40
            if (r, g, b) in ((83, 151, 153), (63, 120, 135)):
                region[p] = 3 if y < (18, 20, 21, 25, 24, 24, 24, 21)[f] else 2
            elif (r, g, b) in ((78, 39, 35), (167, 103, 62), (161, 185, 167)):
                region[p] = 1
            elif (r, g, b) in ((113, 106, 83), (45, 40, 36)):
                region[p] = 4
            elif (r, g, b) in ((0, 0, 0), (31, 25, 25), (37, 40, 45)):
                if x < 14 and y > 17:
                    region[p] = 4
            else:
                raise ValueError(('Unassigned authored color', f, p, pixels[p]))
        colored = [p for p, r in enumerate(region) if r]
        for p, pixel in enumerate(pixels):
            if pixel[3] and not region[p]:
                nearest = min(colored, key=lambda q: ((p%40-q%40)**2+(p//40-q//40)**2, q))
                region[p] = region[nearest]
        regions.append(region)
    # Match only within semantic material regions. Each frame has unique owners;
    # newly revealed texels reserve unused rest cells and remain owned when hidden.
    canonical = [0] * 1600
    sources = []
    for region in regions:
        mapping = [0] * 1600
        for r in range(1, 6):
            destinations = {p for p, value in enumerate(region) if value == r}
            available = {p for p, value in enumerate(canonical) if value == r}
            pairs = sorted(((p%40-q%40)**2+(p//40-q//40)**2, p, q)
                           for p in destinations for q in available)
            for _, p, q in pairs:
                if p in destinations and q in available:
                    mapping[p] = q+1
                    destinations.remove(p)
                    available.remove(q)
            for p in sorted(destinations):
                free = min((q for q, value in enumerate(canonical) if not value),
                           key=lambda q: ((p%40-q%40)**2+(p//40-q//40)**2, q))
                canonical[free] = r
                mapping[p] = free+1
        sources.append(mapping)
    return {'source_sha256': hashlib.sha256(SOURCE.read_bytes()).hexdigest(),
            'regions': regions, 'sources': sources, 'canonical_regions': canonical,
            'policy': 'Explicit semantic regions; nearest one-to-one rest correspondence within region. '
                      'Occluded parcels retained. Tail is dark left appendage; cyan upper hat/lower robe; '
                      'brown face/paws fur. Teeth are not separately visible in this source.'}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--author-maps', action='store_true')
    parser.add_argument('--author-cast-maps', action='store_true')
    args = parser.parse_args()
    doc = read(SOURCE, {'Layer 1': 'color'})
    if doc['canvas'] != [40, 40] or len(doc['frames']) != 8:
        raise ValueError('Expected eight authored 40x40 frames')
    if any(f['duration_ms'] != 100 for f in doc['frames']):
        raise ValueError('Review phased timing before importing changed durations')
    frames = [f['channels']['color'] for f in doc['frames']]
    if args.author_maps:
        MAP.write_text(json.dumps(author_maps(frames), separators=(',', ':'))+'\n')
    maps = json.loads(MAP.read_text())
    if maps['source_sha256'] != hashlib.sha256(SOURCE.read_bytes()).hexdigest():
        raise ValueError('Source changed; review material and correspondence maps')
    for f, rgba in enumerate(frames):
        used = set()
        for p in range(1600):
            source = maps['sources'][f][p]
            region = maps['regions'][f][p]
            if bool(source) != bool(rgba[p*4+3]) or bool(region) != bool(source):
                raise ValueError('Coverage mismatch')
            if source:
                if source in used or maps['canonical_regions'][source-1] != region:
                    raise ValueError('Duplicate or material-changing owner')
                used.add(source)
    cast = read(CAST, {'Layer 1': 'color'})
    if cast['canvas'] != [80, 40] or len(cast['frames']) != 8 or any(
            f['duration_ms'] != 100 for f in cast['frames']):
        raise ValueError('Expected eight 80x40 cast frames at 100ms')
    cast_frames = [f['channels']['color'] for f in cast['frames']]
    if args.author_cast_maps:
        CAST_MAP.write_text(json.dumps(cast_maps(cast_frames, maps['canonical_regions']),
                                       separators=(',', ':'))+'\n')
    cm = json.loads(CAST_MAP.read_text())
    if cm['source_sha256'] != hashlib.sha256(CAST.read_bytes()).hexdigest():
        raise ValueError('Cast source changed; review maps')
    if any(r and r != cm['canonical_regions'][p] for p, r in enumerate(maps['canonical_regions'])):
        raise ValueError('Existing canonical identity changed')
    for f, rgba in enumerate(cast_frames):
        used = set()
        for p in range(3200):
            source, region = cm['sources'][f][p], cm['regions'][f][p]
            if bool(source) != bool(rgba[p*4+3]) or bool(region) != bool(source):
                raise ValueError('Cast coverage mismatch')
            if source:
                if source in used or cm['canonical_regions'][source-1] != region:
                    raise ValueError('Duplicate or material-changing cast owner')
                used.add(source)
    def array(name, ctype, rows):
        return 'static const '+ctype+' '+name+'['+str(len(rows))+']['+str(len(rows[0]))+']={\n'+''.join(
            '    {'+','.join(map(str, row))+'},\n' for row in rows)+'};\n'
    def contacts(regions, width, baselines):
        result = []
        for f, baseline in enumerate(baselines):
            feet = [p % width for p, region in enumerate(regions[f])
                    if region == 1 and p // width == baseline - 1]
            if not feet:
                raise ValueError('Authored fur-foot baseline has no foot pixels')
            result.append([min(feet), max(feet) + 1, baseline])
        return result
    content = '/* Generated by scripts/props/beaver_import.py; preserve source ASE. */\n'
    content += array('beaver_rgba', 'uint8_t', frames)
    content += array('beaver_sources', 'uint16_t', maps['sources'])
    content += array('beaver_cast_rgba', 'uint8_t', cast_frames)
    content += array('beaver_cast_sources', 'uint16_t', cm['sources'])
    content += array('beaver_regions', 'uint8_t', [cm['canonical_regions']])
    # Reviewed contacts use actual paws, excluding tail/staff and spell extent.
    content += array('beaver_chomp_contacts', 'uint16_t', contacts(
        maps['regions'], 40, [32] * 8))
    content += array('beaver_cast_contacts', 'uint16_t', contacts(
        cm['regions'], 80, [32, 33, 33, 33, 33, 32, 32, 32]))
    HEADER.write_text(content)


if __name__ == '__main__':
    main()
