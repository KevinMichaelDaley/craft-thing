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
Each leaf flake carries its own source sRGB through motion and persistence, and
renders at full coverage despite its low mass. Flakes use a half-cell contact
radius rather than the former quarter-cell powder radius.

Imported, generated and painted soil/gravel start with one full cell of rest volume: mass equals
material density (1.2 for soil, 2.7 for gravel), identity deformation, and half-cell
grain radius. Root support history includes granular neighbors, so removing the
soil or gravel under a tree releases its entire connected wood/leaf component.
Older scene saves recover missing granular support history without restamping.
Rigid contacts use each grain's actual position, radius and particle layer.
Loose grains exchange corrections and impulses with bodies using their finite
mass, so trapped dirt cannot act as an immovable anchor inside a falling trunk.
Granular transfer reserves rest volume rather than allowing two full-cell
soil/gravel grains to collapse into one cell. Limited transient compression
still allows smaller sand grains to sift through a mixture. Natural partial
support motion keeps loaded terrain fixed while original granular support
remains; a direct cut releases the affected component for contact simulation.
Rigid contact relaxation accounts for grains' small share of body response and
uses binding contacts, preserving stone support under dense leaf contacts.
The launcher uses `world_chunks_level_piece03_packed`; earlier worlds remain in
`world_chunks_level_piece03` and `world_chunks_level_piece03_dense` with edits intact.

`build/dense_support_tests` checks 64-tick soil/gravel packing, partial support
motion versus a direct cut, and dense-leaf contacts against a stone floor.
`build/level03_support_tests` uses the actual scene in a Wayland window: 90
untouched ticks, a trunk cut, then 240 physics ticks with stone-overlap checks.
It writes `build/level03_packed.bmp` and `build/level03_tree_stopped.bmp`.

Rebuild (Python with Pillow):

```sh
python3 scripts/props/png_pair_export.py assets/props/level_piece03.png \
  assets/props/level_piece03_mat.png \
  --profile assets/props/profiles/level_piece03.json \
  --output assets/props/level_piece03.prop
```

Run `./tools/demo_level_piece03`. Physics runs automatically; P pauses it.
