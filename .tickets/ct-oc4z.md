---
id: ct-oc4z
status: in_progress
deps: []
links: [ctlo-inhv]
created: 2026-11-01T18:33:33Z
type: bug
priority: 1
assignee: kmd
tags: [vulkan, performance, catapult]
---
# Profile and reduce RTX catapult scene startup before first rendered frame

On 2026-11-01, the real DISPLAY=:1 NVIDIA RTX A2000 catapult_demo created a 1024x512 desktop window but took roughly two minutes at about one CPU core before reporting game view ready. Focused catapult scene and Xephyr display tests similarly spend tens of seconds before first frame. The user expects a visible, responsive demo on launch.

## Design

Instrument GPU creation, pipeline compilation, scene upload, structural placement, combustion setup, renderer enable, and first submit independently; retain timing traces in a checked-in diagnostic. Use cached or deferred initialization where supported without changing physics or output.

## Acceptance Criteria

A cold and warm catapult_demo on the actual RTX display render the first frame quickly, with measured setup phases and no blank desktop window for tens of seconds. Keep the 256x128 game resolution and full launch/ignition behavior; focused tests and full make test pass.


## Notes

**2026-11-27T00:32:27Z**

The catapult executable now has a first-frame startup probe on its actual 256x128 load, fixed launch scene and lit presentation path. Run make test_catapult_startup with an X11 Vulkan-capable display, or run ./build/catapult_demo --startup-probe directly. DC_BONFIRE_STARTUP_TRACE=1 also exposes the existing device, shader-pipeline and swapchain milestones. Use DC_PIPELINE_CACHE_DIR and DC_PIPELINE_CACHE_TRACE=1 to compare a fresh and then warmed pipeline cache. The portable regression reproduces and verifies the launch marker on an unavailable display. RTX cold and warm first-present evidence and startup reduction remain open.
