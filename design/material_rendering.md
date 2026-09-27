# Density-based material antialiasing

The normal Vulkan compute renderer reconstructs a coverage value and a
premultiplied linear-light color for every simulated cell. Stone and GPU rigid
occupancy are opaque. Sand, dirt, and gravel use the sum of their fixed-point
particle masses as coverage, clamped to one cell; when both slots are occupied,
their colors are mixed by mass. Mud and wet sand select their material variants
before mixing. Eulerian water uses its fixed-point cell mass as coverage and
composites over granular material. Rendering reads these buffers without
changing their simulation state or downloading them to the CPU.

A separable three-tap filter with weights 1/8, 3/4, 1/8 on each axis blends
these material densities across pixel edges. Every 16×16 workgroup loads one
18×18 neighborhood into shared memory, including cells in adjacent resident
chunks through the GPU page table. Missing pages and the viewport exterior
contribute empty coverage. The filtered result is composited over the dark
background in approximate linear light, then converted back to display RGB.
Uniform material interiors keep their original colors; fractional water,
single-cell features, and material boundaries receive partial coverage.

The brush write mode retains direct cell rendering because neighboring cells
can be edited concurrently during that dispatch. The following normal render
uses the filtered path. Marker, resident-chunk, and diagnostic overlays remain
exact colors so their meaning does not change. This is visual reconstruction
at the simulation's cell resolution; it does not alter water projection,
particle mass, material IDs, or chunk streaming.
