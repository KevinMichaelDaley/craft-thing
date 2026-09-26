# Dungeoncraft

Dungeoncraft is an early C11/Vulkan prototype for a per-pixel material world. The current vertical slice streams procedural chunks on a worker thread, maps a bounded chunk atlas through a GPU page table, draws it with a SPIR-V compute shader, and presents it through a Vulkan swapchain. The window accepts material painting; the rigid-body, Eulerian fluid, and falling-sand simulation passes are still to come.

## Build and run

Install development packages for Vulkan, SDL2, `pkg-config`, and `glslangValidator` (Ubuntu packages: `libvulkan-dev`, `libsdl2-dev`, `glslang-tools`). Then run:

```sh
make all
make test
make test_ui
./build/dungeoncraft
```

`make test` runs headless Vulkan compute readback and chunk/worker persistence tests. `make test_ui` opens a Vulkan window, paints material on the GPU, pans away, and verifies the edit after streaming the chunk back in; it also checks negative world coordinates. It needs a graphical display. Set `DC_VK_VALIDATE=1` to request `VK_LAYER_KHRONOS_validation`; the app reports a clear error if that optional layer is not installed. Set `GLSLANG=/path/to/glslangValidator` when the compiler is outside `PATH`.

In the window, drag the left mouse button to paint and the right button to erase. Keys `1`, `2`, and `3` select stone, sand, and water; `0` selects erasing. Arrow keys or WASD pan by one chunk; Escape closes the window. Use `./build/dungeoncraft --seed 1234` to start a different procedural world. Edits are stored under `world_chunks/`.

The world uses signed 64-bit chunk coordinates, 64 x 64 cell chunks, 64 GPU resident slots, and a separate C11 worker thread for procedural generation and disk load/save. Seeded generation produces continuous terrain, caves, and a fluid basin across chunk borders. The main thread owns Vulkan uploads and readbacks; the worker never calls Vulkan. The current vertical slice waits for each GPU dispatch to complete, so asynchronous GPU staging and performance optimization remain future work. The [engine plan](ENGINE_PLAN.md) and `.tickets/` track the simulation stages and remaining testbed controls.
