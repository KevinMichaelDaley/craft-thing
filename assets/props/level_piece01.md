# Level piece 01

Source: `level_piece01.aseprite`, copied byte-for-byte from the prop-export
worktree. Its SHA256 is recorded in the exported manifest. The original `.ase`
file is a separate revision and was not substituted for the requested `.aseprite`.

Export again from the repository root:

```sh
python3 scripts/props/profile_export.py assets/props/level_piece01.aseprite \
  --profile assets/props/profiles/level_piece.json \
  --output assets/props/level_piece01.prop
python3 scripts/props/pack_prop.py --validate assets/props/level_piece01.prop
```

The 128x128 document has one 100 ms frame. `Layer 1` is color; `Layer 2`
maps exact opaque red to wood, black to stone, yellow to dirt (soil), and blue
to water. `Layer 3` maps exact opaque white to terrain. All 8,655 physical pixels
have the terrain role: wood 761, stone 5,096, dirt 1,294, water 1,504.

The remaining 7,729 source positions have no placement. Transparent material
means preserve the existing world, including its appearance; the export clears
all channels at those positions, even if the role/art layers contain a pixel.
It does not encode an instruction to erase the world to air. GPU placement skips
all absent pixels and rejects the whole placement if an occupied destination is
unknown, overlaps a body, or uses a role unsupported by this terrain path.

The source includes 10 occupied material pixels without visible color. Material
metadata remains authoritative: this profile explicitly allows those invisible
physical pixels instead of painting invented artwork or deleting physics.
Unknown and partial-alpha metadata colors are rejected. The source is never
renamed, flattened, recolored, or saved over.

The package contains validated material/role/object/anchor planes, color PNG,
compiled `asset.cprop`, and a manifest with source/profile provenance. Its hashed
`ground.bin` companion defines the physical surrounding ground line. Fresh chunks
leave air above it and 12 cells of soil below it, then stone; the edge heights
extend into neighboring chunks. Saved player edits are preserved. This prevents
the ordinary procedural stone surface from burying the authored scene. The source
image/material mask is unchanged.

Launch the material placement demo with `./tools/demo_level_piece01`. The scene
is centered on world chunk `(0,0)`; its full 128x128 footprint starts at world
`(-32,-32)` and crosses neighboring 64x64 chunks. It opens paused in authored
albedo mode. Press P to run/pause, N to single-step, 0–6 to select air/stone/sand/
water/dirt/gravel/wood, and use left/right mouse to paint/erase. Existing camera,
zoom and physics overlays remain available. F8 restores authored albedo display.

`make test_scene_placement` verifies central-chunk content, matched physical
ground at both exterior joins, stable paused frames, real brush editing and
12 physics ticks; captures are under `build/screenshots/level_piece01_*.bmp`.

This terrain placement path publishes physical pixels and current resident
appearance together. General rigid/structural authored publication, moving/
fractured appearance ownership and save/reload of authored appearance remain
tracked by `ctpf-iueo`, `ctpe-847v`, and `ctpe-d6m2`.
