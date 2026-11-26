"""Pack the rooted tree from level piece 03 as a standalone terrain prop."""

import hashlib
import struct
from pathlib import Path

from PIL import Image

from pack_prop import MATERIALS, export, validate


PROPS = Path(__file__).resolve().parents[2] / "assets" / "props"
SOURCE = PROPS / "level_piece03.prop"
DESTINATION = PROPS / "spawn_tree.prop"
BOUNDS = (166, 0, 259, 135)
TREE_MATERIALS = (2, 10)


def main():
    source_color = SOURCE / "0000.color.png"
    source_material = SOURCE / "0000.material.u16"
    physical = source_material.read_bytes()
    left, top, right, bottom = BOUNDS
    width, height = right - left, bottom - top
    with Image.open(source_color) as image:
        source_width = image.width
        color = bytearray(image.convert("RGBA").crop(BOUNDS).tobytes())
    material = bytearray(width * height * 4)
    role = bytearray(width * height * 4)
    for y in range(height):
        for x in range(width):
            index = (top + y) * source_width + left + x
            (kind,) = struct.unpack_from("<H", physical, index * 2)
            pixel = (y * width + x) * 4
            if kind in TREE_MATERIALS:
                material[pixel:pixel + 4] = bytes(MATERIALS[kind]["rgba"])
                role[pixel:pixel + 4] = b"\x00\xff\x00\xff"
            else:
                color[pixel:pixel + 4] = bytes(4)

    document = {
        "canvas": [width, height],
        "frames": [{"duration_ms": 100, "channels": {
            "color": bytes(color), "physics.material": bytes(material),
            "physics.role": bytes(role),
        }}],
    }
    provenance = {
        "crop": list(BOUNDS),
        "source_modified": False,
        "sources": [{
            "source": str(path.relative_to(PROPS)),
            "sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
        } for path in (source_color, source_material)],
        "retained_materials": ["wood", "leaves"],
    }
    export(document, DESTINATION, {"authoring": provenance})
    validate(DESTINATION)


if __name__ == "__main__":
    main()
