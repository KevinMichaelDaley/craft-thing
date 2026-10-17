"""Export numbered ASE layers using an explicit exact-label authoring profile."""
import argparse
import copy
import hashlib
import json
from pathlib import Path

from ase_reader import read
from pack_prop import SCHEMA, canonical, export, validate


def normalize(document, profile_path):
    profile = json.loads(Path(profile_path).read_text())
    if profile.get('version') != 1 or profile.get('transparent_material') != 'preserve_existing':
        raise ValueError('Unsupported authoring profile or transparency policy')
    palettes = {}
    for channel, table in (('physics.material', 'materials'), ('physics.role', 'roles')):
        canonical_labels = {entry['name']: bytes(entry['rgba']) for entry in SCHEMA[table]}
        palettes[channel] = {bytes(int(v) for v in label.split(',')): canonical_labels[name]
                             for label, name in profile[table].items()}
    result = copy.deepcopy(document)
    for index, frame in enumerate(result['frames']):
        channels = frame['channels']
        if not all(name in channels for name in ('color', 'physics.material', 'physics.role')):
            raise ValueError('Profile requires color, material and role layers')
        channels = {name: bytearray(raw) for name, raw in channels.items()}
        for at in range(0, len(channels['physics.material']), 4):
            location = f'frame {index} pixel {at // 4}'
            material = bytes(channels['physics.material'][at:at + 4])
            if not material[3]:
                # An absent material means no placement, including no art overwrite.
                for raw in channels.values():
                    raw[at:at + 4] = bytes(4)
                continue
            for name, palette in palettes.items():
                label = bytes(channels[name][at:at + 4])
                if label not in palette:
                    raise ValueError(f'{location}: unknown or partial-alpha {name} label {list(label)}')
                channels[name][at:at + 4] = palette[label]
        frame['channels'] = {name: bytes(raw) for name, raw in channels.items()}
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('--profile', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    source = args.source.read_bytes()
    profile = json.loads(args.profile.read_text())
    document = read(args.source, layer_names=profile['layers'])
    if args.source.read_bytes() != source:
        raise ValueError('Source changed during export; rerun from the saved revision')
    provenance = {'source': args.source.name, 'source_sha256': hashlib.sha256(source).hexdigest(),
                  'profile': profile, 'profile_sha256': hashlib.sha256(args.profile.read_bytes()).hexdigest(),
                  'transparent_material': 'preserve_existing',
                  'soil_material': 'dirt', 'source_modified': False}
    export(normalize(document, args.profile), args.output,
           {'allow_invisible_physics': profile.get('allow_invisible_physics', False),
            'authoring': provenance})
    validate(args.output)
    print('Validated prop package:', args.output)


if __name__ == '__main__':
    main()
