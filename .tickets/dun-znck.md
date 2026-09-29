---
id: dun-znck
status: in_progress
deps: []
links: []
created: 2026-09-29T07:32:05Z
type: bug
priority: 0
assignee: kmd
parent: dun-9e5b
tags: [fluid, gpu, pressure]
---
# Stabilize a 32-cell-high GPU river after a surface displacement

Large rivers remain perpetually bouncy despite per-face viscosity and wall friction. Add a two-chunk river 32 cells deep, disturb its surface, capture rendered before/1s/4s frames, and measure late vertical speed, surface overshoot, and exact mass. Fix the large-domain pressure/hydrostatic cause on GPU.

## Acceptance Criteria

The 32-cell-deep river loses its oscillation over four seconds without large high splashes, preserves exact water mass, crosses the resident chunk seam, and retains interactive half-native tick performance. No per-frame CPU readback.

