#!/usr/bin/env python3
"""Label/enlarge actual Vulkan captures; never synthesize simulation pixels."""
import argparse
from pathlib import Path
from PIL import Image, ImageDraw


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--captures", type=Path, default=Path("build/screenshots"))
    parser.add_argument("--output", type=Path, default=Path("build/screenshots/body_surface_contact_sheet.png"))
    args = parser.parse_args()
    sheet = Image.new("RGB", (864, 756), "#171717")
    draw = ImageDraw.Draw(sheet)
    controls = [(18, "Dry: actual reacting patch"), (28, "Hot inert gas: no reaction"), (38, "Wet: no reaction")]
    for column, (_, label) in enumerate(controls):
        draw.text((column * 288 + 8, 8), label, fill="white")
    for row, (name, label) in enumerate([
        ("before", "Before admitted reaction"),
        ("after", "After finite fuel consumption"),
        ("rotated", "Paused: dry body rotated; rest marks retained"),
    ]):
        y = 32 + row * 232
        draw.text((8, y), label, fill="#c8c8c8")
        with Image.open(args.captures / f"body_surface_{name}.bmp") as frame:
            for column, (x, _) in enumerate(controls):
                crop = frame.convert("RGB").crop((x, 18, x + 8, 24))
                sheet.paste(crop.resize((256, 192), Image.Resampling.NEAREST), (column * 288 + 8, y + 22))
    draw.text((8, 736), "Actual Vulkan pixels, x32 nearest-neighbor; authored grain retained. No mock-up.", fill="white")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(args.output)
    print(args.output)


if __name__ == "__main__":
    main()
