---
id: dun-cwhn
status: open
deps: [dun-d8gb, dun-o1go]
links: []
created: 2026-09-26T05:25:20Z
type: task
priority: 2
assignee: kmd
parent: dun-oa1i
tags: [world, gpu]
---
# Implement chunk halos and safe cross-boundary transfers

Refresh neighbor data for each stencil substep and define missing-neighbor behavior for rigid occupancy, fluid flux, and material movement.

## Acceptance Criteria

Generic particle and scalar transfer tests preserve totals at resident chunk edges. Transfers toward an absent chunk remain pending until it loads; synthetic cross-boundary results match an equivalent unpartitioned grid. Fluid and sand stages add their own integration tests.
