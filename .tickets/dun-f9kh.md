---
id: dun-f9kh
status: closed
deps: []
links: []
created: 2026-09-29T05:09:50Z
type: task
priority: 2
assignee: kmd
parent: dun-9det
tags: [rendering, mpm, materials]
---
# Make wet mud visible in mixed mud-water cells

The new mud MPM scene moves coherently, but after two seconds the shared mud-water shelf renders mostly blue with only sparse brown pixels. The material blend currently hides much of the wet dirt mass.

## Design

Use per-cell dirt particle occupancy and Eulerian water fraction to shade mud-water mixtures in the Vulkan renderer. Restrict blending to materials in the same cell; preserve sharp boundaries.

## Acceptance Criteria

In the interactive --demo-mud scene, a wet connected mound is visibly brown/muddy during flow and water remains visually distinct; before/after screenshots and a GPU render test verify mixed-cell colors without border blur.


## Notes

**2026-09-29T05:32:51Z**

Generated a coordinated OKLCH palette at 16-bit preview precision, used its swatches for GPU per-cell rendering, and kept concentration blending continuous: full mud red exceeds water by at least 55, 1/128-cell dirt differs by at most 12 total RGB units. No neighbor tint. make test, make test_ui, make test_half_native all pass; half-native mud smoke 60.80 ticks/s and 2.777 ms MPM stage.
