---
id: ct-l35t
status: closed
deps: []
links: []
created: 2026-10-05T04:05:20Z
type: task
priority: 2
parent: dun-5kye
tags: [gpu, rigid, geometry]
---
# Store and rasterize convex rigid shapes on Vulkan

Prerequisite for dun-ci2x: represent pre-rotated convex polygons in body-local fixed-point coordinates, with stone/wood material, in the existing bounded GPU body pool.

## Design

Preserve box API and conservative swept AABB broadphase. Rasterize polygon-versus-one-cell overlap on Vulkan, including edge cells. Persist shape records with backwards-compatible box snapshot loading. Contact generation and solving remain in dun-ci2x and dun-9qub.

## Acceptance Criteria

Rotated stone/wood polygons rasterize exact occupied cells across chunk seams, reject malformed or non-convex geometry without mutation, retain conservative broadphase candidates, survive camera rebases and snapshots, and clear shapes on slot reuse.


## Notes

**2026-10-05T04:21:26Z**

Completed with RED/GREEN commits for six headless geometry tests and the new actual quarter-native window scene. Final make test: 114 passed; quarter-native configuration/cadence/solver/window: 37 passed; viewport smoke passed; controls: 5 passed and all 10 UI scenes passed. Ran Vulkan validation on Intel Iris Xe and spirv-val; verified std430 stride 152 and shape offset 80. Shape storage adds 4.5 KiB and no descriptor or routine readback. Rotations are baked into bounded convex vertices; custom polygons translate/fall but contact generation/response and runtime angular corrections remain in dun-ci2x/dun-9qub.
