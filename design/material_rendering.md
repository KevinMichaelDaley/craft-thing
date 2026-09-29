# Density-based material antialiasing

The normal Vulkan compute renderer reconstructs a coverage value and a
premultiplied linear-light color for every simulated cell. Stone and GPU rigid
occupancy are opaque. Sand, dirt, and gravel use the sum of their fixed-point
particle masses as coverage, clamped to one cell; when both slots are occupied,
their colors are mixed by mass. Mud and wet sand select their material variants
before mixing. Eulerian water uses its fixed-point cell mass as coverage and
composites over granular material. Rendering reads these buffers without
changing their simulation state or downloading them to the CPU.

Each output pixel reads only its corresponding simulation cell. It composites
that cell's granular and water coverage over the dark background in approximate
linear light, then converts back to display RGB. Fully covered cells keep their
exact material color. Fractional water and multiple granular particles mix
within one cell; adjacent cells cannot tint each other. This preserves sharp
pixel-scale material borders at native display resolution.

The brush write mode retains direct cell rendering. The following normal render
uses within-cell coverage. Marker, resident-chunk, and diagnostic overlays remain
exact colors so their meaning does not change. This is visual reconstruction
at the simulation's cell resolution; it does not alter water projection,
particle mass, material IDs, or chunk streaming.

The material swatches in `shaders/sim/material_palette.glsl` come from
`python3 tools/generate_material_palette.py`. The generator specifies stone,
soil, sand, gravel, and water in OKLCH, converts with the public-domain
[Oklab transform](https://bottosson.github.io/posts/oklab/), and writes a
1024-sample, 16-bit-per-channel preview to
`build/material_palette_16.ppm`. The shader uses the generated display colors
and its existing linear-light per-cell coverage blend. A full mud cell stays
warm brown; a trace of dirt in a water-filled cell approaches the water color
continuously. No neighboring pixel contributes color. Brush painting now uses
the same reconstruction as the normal view, while diagnostic overlays retain
their fixed colors.
