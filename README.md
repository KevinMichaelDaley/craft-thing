# Dungeoncraft

Dungeoncraft is an early C11/Vulkan prototype for a per-pixel material world. The current vertical slice dispatches a SPIR-V compute shader, reads its cell buffer back for tests, and presents that buffer through a Vulkan swapchain. The window accepts mouse painting; the streamed procedural world is not yet connected to the renderer.

## Build and run

Install development packages for Vulkan, SDL2, `pkg-config`, and `glslangValidator` (Ubuntu packages: `libvulkan-dev`, `libsdl2-dev`, `glslang-tools`). Then run:

```sh
make all
make test
make test_ui
./build/dungeoncraft
```

`make test` runs headless Vulkan compute readback and chunk/worker persistence tests. `make test_ui` opens a Vulkan window for a two-frame presentation and brush smoke test, so it needs a graphical display. Set `DC_VK_VALIDATE=1` to request `VK_LAYER_KHRONOS_validation`; the app reports a clear error if that optional layer is not installed. Set `GLSLANG=/path/to/glslangValidator` when the compiler is outside `PATH`.

In the window, drag the left mouse button to paint. Keys `1`, `2`, and `3` select colors; `C` resets the diagnostic pattern; Escape closes the window.

The world code already defines signed 64-bit chunk coordinates, 64 x 64 cell chunks, bounded resident-slot state, and a separate C11 worker thread for procedural generation and disk load/save. Seeded generation produces continuous terrain, caves, and a fluid basin across chunk borders. End-to-end tests verify that generated cells arrive through the worker and edits survive save, eviction, and reload. The [engine plan](ENGINE_PLAN.md) and `.tickets/` track the remaining GPU page table, simulation stages, and interactive procedural level work.
