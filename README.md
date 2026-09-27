# Dungeoncraft

Dungeoncraft is an early C11/Vulkan prototype for a per-pixel material world. The current vertical slice streams procedural chunks on a worker thread, maps a bounded chunk atlas through a GPU page table, draws it with a SPIR-V compute shader, and presents it through a Vulkan swapchain. The window accepts material painting, runs one GPU rigid box, moves Eulerian water on fixed grid cells, and settles sand, dirt, and gravel through a GPU particle/grid MPM stage.

The live frame records rigid simulation, a GPU red-black pressure projection, conservative four-phase water transport, sparse interface-marker advection and correction, granular MPM, and rendering in one Vulkan submission. Water volume stays in fixed grid cells; GPU face velocities move it. Massless inside/outside markers guide bounded equal-and-opposite transfers near settled interfaces. Timestamp and buffer readback are opt-in diagnostics, while chunk state is downloaded only for streaming saves or tests. See [the fluid solver plan](design/fluid_solver.md) and [the granular solver](design/mpm_solver.md).

GPU chunk halos are bounded by the 64 resident slots and refreshed from the page table before a stencil transfer. A diagnostic transfer resolver moves one scalar amount or material particle between adjacent cells. It preserves the command and source state if the destination page is absent, then applies it after that page is mapped. Fluid uses conservative face transport; MPM gathers across resident page-table neighbors and blocks moves into missing pages. The resolver remains a single-command correctness primitive.

## Build and run

Install development packages for Vulkan, SDL2, `pkg-config`, and `glslangValidator` (Ubuntu packages: `libvulkan-dev`, `libsdl2-dev`, `glslang-tools`). Then run:

```sh
make all
make test
make test_ui
make test_ui_long
./build/dungeoncraft
```

`make test` runs headless Vulkan compute readback, rigid motion/contact, Eulerian water conservation, deterministic marker ownership and reload, and chunk/worker persistence tests. It saves marker-on/off thin-sheet and splash captures under `build/screenshots/`. `make test_ui` checks the streamed window and saves rendered screenshots at `build/screenshots/before.bmp` and `build/screenshots/after_1s.bmp`, separated by 60 fixed simulation ticks. It asserts that falling water reaches well below its source and that markers survive GPU chunk eviction and worker reload. It needs a graphical display. Set `DC_VK_VALIDATE=1` to request `VK_LAYER_KHRONOS_validation`; the app reports a clear error if that optional layer is not installed. Set `GLSLANG=/path/to/glslangValidator` when the compiler is outside `PATH`.

`make test_ui_long` runs the windowed fluid scene for 600 presented ticks, paints water at tick 361, and saves `build/screenshots/after_5s.bmp` and `after_10s.bmp`. It checks that the images differ, including near the painted area. The window remains visible while the test runs; use `./build/dungeoncraft` for an open-ended interactive session.

Run `./build/dungeoncraft --demo-coupled --world-dir build/my_coupled_world` for an open-ended mud test. It paints a supported dirt layer over the generated floor and places water, dirt, sand, and gravel above it. The water and grain GPU velocities lose 0.1% per solve step to settle residual bouncing. `make test_ui` also saves the combined scene as `build/screenshots/coupled_before.bmp` and `build/screenshots/coupled_after_1s.bmp`.

In the window, a world-anchored spring above the center basin emits water so there is always falling motion to inspect. Drag the left mouse button to paint and the right button to erase. Keys `1` through `5` select stone, sand, water, dirt, and gravel; `0` selects erasing. Sand, dirt, and gravel fall and settle through the GPU MPM stage. Press `B` to spawn the current rigid test box at the pointer, `P` to pause or resume, `N` to advance one tick, and `M` to toggle the colored inside/outside marker overlay. The box moves right and falls onto solid terrain. Hold arrow keys or WASD to pan smoothly, one world cell at a time across chunk boundaries. Use `-` and `+` to switch between exact 1×, 2×, and 4× nearest-neighbor display; the smaller views are centered and mouse painting follows the displayed cells. Press `V` to cycle normal view, green resident-chunk borders, and a rigid/fluid/sand cell-output overlay. The window title reports seed, camera position, and resident chunk count. Press `R` for a fresh procedural run with the same seed, or `F2` to type a new seed and press Enter; each fresh run gets its own directory, leaving the previous world saved. Escape closes the window. Use `./build/dungeoncraft --seed 1234` to start a different procedural world. Edits are stored under `world_chunks/`.

The world uses signed 64-bit chunk coordinates, 64 x 64 cell chunks, 64 GPU resident slots, and a separate C11 worker thread for procedural generation and disk load/save. Each chunk stores up to 2048 markers; version 2 chunk files preserve them, and version 1 files still load. Seeded generation produces continuous terrain, caves, and a fluid basin across chunk borders. The main thread owns Vulkan uploads and save readbacks; the worker never calls Vulkan. Rigid motion currently supports one viewport-local box, gravity, and downward terrain contact; additional bodies, world-space streaming, horizontal contact, and swept occupancy remain ticketed work. The live frame uses one GPU submission and one completion wait. The [engine plan](ENGINE_PLAN.md) and `.tickets/` track the simulation stages and remaining testbed controls.
