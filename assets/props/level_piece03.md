# Level piece 03

Source color: `level_piece03.png`; physics: `level_piece03_mat.png`.
Both source PNGs remain unchanged. The import uses the common occupied rectangle
`[296,383,735,711]`, producing a 439×328 scene without resampling or changing the
relative alignment of any pixels. Empty material pixels preserve the level.

The explicit profile is `profiles/level_piece03.json`: white = soil, magenta =
gravel, black = stone, red = wood, green = leaves. Seven stray yellow pixels map
to soil and 31 artwork-colored metadata pixels map to stone. Every occupied
pixel starts as terrain. Surrounding ground follows the authored soil surface.

Leaves are a distinct connected solid with density 0.2 relative to water. On
breaking a rigid canopy, leaves crumble into small independent leaf-debris
particles; wood remains wood. Leaf material survives extraction and save/load.

Rebuild (Python with Pillow):

```sh
python3 scripts/props/png_pair_export.py assets/props/level_piece03.png \
  assets/props/level_piece03_mat.png \
  --profile assets/props/profiles/level_piece03.json \
  --output assets/props/level_piece03.prop
```

Run `./tools/demo_level_piece03`. Physics runs automatically; P pauses it.
