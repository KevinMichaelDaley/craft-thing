---
id: dun-kq6u
status: closed
deps: []
links: []
created: 2026-09-27T21:17:32Z
type: task
priority: 2
assignee: kmd
parent: dun-v2vu
tags: [gpu, renderer, materials]
---
# Density-based antialiasing for every material

Render stone, sand, dirt, gravel, water, and rigid occupancy from GPU material coverage rather than one flat cell color; blend coverage across material edges and shared particle slots.

## Design

Use a compact spatial reconstruction in the existing SPIR-V render compute pass. Coverage comes from Eulerian water mass, particle mass, and opaque solids. Keep material diagnostics exact and do not change simulation buffers or add per-frame readback.

## Acceptance Criteria

All materials show coverage-weighted edges; fractional water and mixed grain slots render proportionally; resident chunk seams match interior filtering; uniform interiors retain their base color; screenshots show the result; GPU-only rendering and normal-frame performance remain intact.


## Notes

**2026-09-27T21:27:36Z**

Implemented shared-memory 18x18 GPU density reconstruction for 16x16 render tiles. Stone/rigid use full coverage, water uses Eulerian mass, grains use particle mass and both slots; filtering crosses resident chunk seams. Uniform interiors retain base colors, overlays stay exact, and rendering does not alter simulation buffers. make test, make test_ui, and Vulkan validation pass.
