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

The grid allocation and fluid projection operate at the stated resolution,
but only the 64 mapped chunks contain resident terrain and water. These numbers
therefore do **not** establish fully populated native gameplay throughput. The
fluid stage dominates. Initially its pressure solver dispatched 42 full-grid
passes per tick. It now dispatches pressure and per-cell fluid work by resident
GPU chunk slots; the hydrostatic column scan and directional transport still
cover the full grid, and other passes need further profiling. A fully resident
native viewport also needs a larger GPU chunk pool
and a dynamic interactive view. The current 256×128 interactive view remains
the playable default while those paths are implemented and measured.
