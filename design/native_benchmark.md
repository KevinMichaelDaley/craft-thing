# Native-resolution simulation trial

Run `make bench_native GLSLANG=/tmp/dungeoncraft-tools/usr/bin/glslangValidator`
for the current display's 1920×1080 pixel grid. The benchmark creates a headless
Vulkan context, maps 64 distinct 64×64 chunks, fills a water band in each chunk,
warms up three ticks, then times twelve synchronous GPU ticks and twelve ticks
with a GPU render. It reads timestamps once for rigid, fluid, and granular
stages. Neither loop downloads cell buffers. The benchmark accepts optional
`width height` arguments for other display sizes.

On 2026-09-27, DP-1 was 1920×1080 at 60 Hz and the GPU was an NVIDIA RTX
A2000 12GB. With the current 64-slot resident-chunk cap:

| Grid | Resident cells | Tick wall time | Tick rate | Tick + render | GPU rigid / fluid / granular |
| --- | ---: | ---: | ---: | ---: | ---: |
| 1024×576, before sparse dispatch | 262,144 | 102.97 ms | 9.71 Hz | 103.20 ms | 0.23 / 105.34 / 1.78 ms |
| 1920×1080, before sparse dispatch | 262,144 | 276.15 ms | 3.62 Hz | 278.88 ms | 0.74 / 268.80 / 6.08 ms |
| 1920×1080, resident-page dispatch | 262,144 | 85.33 ms | 11.72 Hz | 84.87 ms | 0.76 / 78.81 / 6.06 ms |

Those headless numbers use only 64 mapped chunks and do **not** establish fully
populated native gameplay throughput. The resident-page pressure dispatch
removed full-grid work from the 42 pressure passes, though hydrostatic columns
and directional transport still cover the grid.

The interactive native build uses `make native` and
`./build/dungeoncraft_native`. It renders 1920×1080 distinct simulated cells
at 1× scale, backed by 608 resident chunks including the halo. Procedural
terrain, basins, caves, surface details, the water source, and the brush are
scaled 4× relative to the earlier 256×128 world. The native build stores chunks
in `world_chunks_native/` by default so old 1× worlds are not mixed with the
new map. [Native screenshot](native_map_1920x1080.png) shows the generated
map after the scaling change.

The native mode spreads one fluid update over six physics ticks. Projection
preparation and pressure iterations occupy the first four ticks; remaining
pressure work and transport run on the fifth; marker correction completes on
the sixth. Rigid and granular work still runs every tick. No cell-buffer
copyback occurs during normal frames; modified chunks are downloaded for
persistence when streamed or when the game closes.

`make test_native` generated and loaded all 608 chunks, verified the last
screen pixel maps to the last simulated cell, painted at the far edge, and
presented twelve physics ticks. On the same RTX A2000, those twelve ticks took
about 1.4 s: **about 8.4 ticks/s**, with roughly 76 ms fastest and 141 ms slowest
frame. They contained two complete fluid updates. Before splitting the fluid
work across ticks, a twelve-tick run reached 6.25 ticks/s and varied from
37.3 to 509.7 ms per frame. Native gameplay is still below the 60 Hz target.
