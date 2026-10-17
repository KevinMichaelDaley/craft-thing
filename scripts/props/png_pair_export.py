"""Import aligned color/material PNGs as an explicitly mapped terrain prop.

Requires Pillow. Transparent material pixels preserve existing level contents.
"""
import argparse
import hashlib
import json
from pathlib import Path

from PIL import Image

from pack_prop import export, validate
from profile_export import normalize


def read_pair(color_path, material_path, crop=None):
    with Image.open(color_path) as source, Image.open(material_path) as metadata:
        if source.size != metadata.size:
            raise ValueError('Color and material dimensions must match; resampling is forbidden')
        if crop is not None:
            if (len(crop) != 4 or any(type(value) is not int for value in crop) or
                    not 0 <= crop[0] < crop[2] <= source.width or
                    not 0 <= crop[1] < crop[3] <= source.height):
                raise ValueError('Invalid lossless crop rectangle')
            source, metadata = source.crop(crop), metadata.crop(crop)
        color = source.convert('RGBA').tobytes()
        material = metadata.convert('RGBA').tobytes()
        return {'canvas': list(source.size), 'mode': 'rgba', 'tags': [], 'frames': [{
            'duration_ms': 100, 'channels': {
                'color': color, 'physics.material': material,
                'physics.role': bytes([255, 255, 255, 255]) * (source.width * source.height)}}]}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('color', type=Path)
    parser.add_argument('material', type=Path)
    parser.add_argument('--profile', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    profile = json.loads(args.profile.read_text())
    provenance = {'profile': profile, 'sources': [
        {'source': path.name, 'sha256': hashlib.sha256(path.read_bytes()).hexdigest()}
        for path in (args.color, args.material)], 'source_modified': False}
    export(normalize(read_pair(args.color, args.material, profile.get('crop')), args.profile), args.output,
           {'allow_invisible_physics': profile.get('allow_invisible_physics', False),
            'authoring': provenance})
    validate(args.output)
    print('Validated prop package:', args.output)


if __name__ == '__main__':
    main()
