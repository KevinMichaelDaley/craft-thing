# Native-resolution simulation trial

Run `make bench_native GLSLANG=/tmp/dungeoncraft-tools/usr/bin/glslangValidator`
for a 1920×1080 pixel grid. The benchmark creates a headless Vulkan context
with a 608-slot pool and maps every viewport page: 510 chunks for 1920×1080,
or 608 for the interactive simulation's 2048×1216 grid including its halo.
The deterministic scene contains a continuous basin, a half-height water fill,
and a submerged 16-cell sand band across the middle half of the viewport.
Partial edge chunks contain no out-of-view water or grains. The output reports
actual mapped residency and initial water and particle counts.

After three warm-up ticks, it averages rigid, fluid, and granular timestamps
over twelve diagnostic ticks. Separate loops time twelve synchronous GPU ticks
without diagnostic readback and twelve ticks with GPU rendering. Neither timed
loop downloads simulation buffers. This headless benchmark runs a complete
fluid update every tick; it does not use the interactive six-tick fluid cadence.
Use `./build/native_bench width height [samples]` for other sizes or sample
counts, for example `./build/native_bench 2048 1216 12`. Scenes needing more
than 608 slots are rejected rather than measured with incomplete residency.

On 2026-10-04, the corrected fully resident benchmark ran through Vulkan on
Intel Iris Xe (TGL GT2), Mesa 26.0.2. Each measurement below uses twelve
samples after three warm-up ticks, with separate diagnostic, physics, and
physics-plus-render loops:

| Grid | Mapped chunks | Initial grains | Tick wall time | Tick rate | Tick + render | GPU rigid / fluid / granular |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| 1920×1080 | 510 | 15,360 | 166.95 ms | 5.99 Hz | 171.10 ms | 0.41 / 132.26 / 27.64 ms |
| 2048×1216 | 608 | 16,384 | 191.28 ms | 5.23 Hz | 196.57 ms | 0.47 / 151.51 / 32.26 ms |

These use full fluid updates on an integrated GPU and cannot be compared
directly with the earlier staged interactive RTX rates. Fluid dominates this
scene; profiling individual hydrostatic, pressure, transport, and marker
passes remains part of `dun-vd9a`. This trial also exposed an out-of-bounds
host velocity copy at partial viewport pages, fixed with regression coverage
in `ct-qk58`. All 86 headless tests passed with Vulkan validation enabled.

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

`make test_native` and `make test_half_native` generate and load the resident
chunks, paint at the far edge, and present sixty physics ticks plus sixty
adaptive-time frames. The half-native mode uses 960×540 simulated cells,
187 resident chunks, and a 2× upscale to the 1920×1080 window. Procedural
features have the same apparent screen size as the native mode. Native mode
uses a six-tick fluid interval; half-native uses three ticks after the longer
step was found to eject spurious free-surface droplets. Normal simulation and
rendering stay on the GPU. Their screenshots are written under `build/screenshots/`.

On the RTX A2000, a paired run on 2026-09-27 measured:

| Interactive mode | Presented ticks/s | Adaptive frames/s | GPU fluid / granular ms per profiled tick |
| --- | ---: | ---: | ---: |
| Native 1920×1080 | 38.9 | 39.4 | 9.2 / 7.3 |
| Half-native 960×540, 2× display | 67.6 | 71.1 | 1.6 / 4.3 |

Those paired rates used the older six-tick cadence. With the three-tick
half-native cadence and 16 red-black pressure sweeps, a 60-frame sample reached
60.4 adaptive frames/s; another run while the older live demo shared the GPU
measured 47.8 frames/s. In a 1,200-tick spring scene followed by 120 ticks
with the spring off, the average count of high airborne water pixels fell from
about 42 at a six-tick fluid interval to 1.5 with marker correction enabled.
The marker-count overflow found during earlier long native runs was fixed in
`dun-mt9o`. Native 1920×1080 still uses six-tick staging; its time-step
stability and 60 Hz performance remain active work in `dun-vd9a`.
