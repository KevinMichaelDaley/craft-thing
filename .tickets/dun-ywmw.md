---
id: dun-ywmw
status: in_progress
deps: []
links: []
created: 2026-09-26T07:46:17Z
type: bug
priority: 1
assignee: kmd
parent: dun-9e5b
tags: [gpu, fluid, visual]
---
# Make Eulerian water fall and spread at a lively rate

The visible spring and collapsed water columns creep sideways like syrup despite a projected velocity field.

## Design

Measure collapse reach at fixed ticks; remove excessive per-tick damping and flux throttling while preserving CFL-safe bounded transfers, pressure projection, exact mass, and GPU-only normal frames.

## Acceptance Criteria

A tall water column spreads materially farther after one simulated second, the interactive 1s/10s captures show a lively falling stream, and conservation, divergence, seam, and Vulkan validation tests still pass.


## Notes

**2026-09-26T08:27:35Z**

Removed half-cell velocity and one-face transport ceilings. Transport distance is face velocity times one fixed tick times 1.001; solid cells and resident-page edges remain physical boundaries. Full suite and 600-tick UI capture pass, but visible surface speckling and projection quality still need work.
