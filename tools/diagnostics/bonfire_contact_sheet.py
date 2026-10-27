#!/usr/bin/env python3
"""Review actual Vulkan flame-test captures; never synthesize simulation pixels."""
import argparse
from pathlib import Path
from PIL import Image, ImageDraw


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--captures", type=Path, default=Path("build/screenshots"))
    parser.add_argument("--output", type=Path,
                        default=Path("build/screenshots/transported_flame_contact_sheet.png"))
    arguments = parser.parse_args()
    labels = ["Horizontal gas flow", "Diagonal open control",
              "Closed roof blocks flow", "Side vent routes flow"]
    sheet = Image.new("RGB", (768, 368), "#202020")
    draw = ImageDraw.Draw(sheet)
    for index, label in enumerate(labels):
        source = arguments.captures / f"transported_flame_{index}.bmp"
        with Image.open(source) as frame:
            crop = frame.convert("RGB").crop((8, 8, 56, 48))
        x = index * 192
        # Preserve the captured pixel shapes with a four-times enlargement.
        crop = crop.resize((192, 160), Image.Resampling.NEAREST)
        sheet.paste(crop, (x, 32))
        draw.text((x + 6, 8), label, fill="white")
        draw.text((x + 6, 204), f"Vulkan capture {index}", fill="#bfbfbf")
        draw.text((x + 6, 222), "World ROI x8..55 y8..47", fill="#bfbfbf")
    draw.text((8, 276), "Actual displayed pixels, nearest-neighbor x4. No flame mask or fixed column.", fill="white")
    draw.text((8, 300), "Only transported hot fuel + oxygen emits; roof and vent act through gas physics.", fill="white")
    arguments.output.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(arguments.output)
    print(arguments.output)
    placement = arguments.captures / "ordinary_bonfire.bmp"
    if placement.is_file():
        with Image.open(placement) as frame:
            frame.save(placement.with_suffix(".png"))


if __name__ == "__main__":
    main()
