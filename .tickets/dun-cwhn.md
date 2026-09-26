---
id: dun-cwhn
status: closed
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

## Notes

**2026-09-26T06:25:04Z**

Implemented slot-bounded one-cell GPU halos, refreshed from the page table for each diagnostic transfer substep. Missing pages mark halo cells nonresident; rigid terrain lookup already treats missing pages as solid. A GPU single-command scalar/particle resolver preserves source and pending state until destination maps, applies exactly once, and blocks invalid/over-capacity transfers. Cross-chunk results match unpartitioned-grid tests; all tests and UI smoke pass with Vulkan validation. World-coordinate pending transfer ownership across camera moves and slot reuse is follow-up dun-abfx; fluid/sand parallel proposal integration remains with their solver tickets.
