---
id: ctlo-qnrt
status: in_progress
deps: []
links: []
created: 2026-10-24T07:36:04Z
type: task
priority: 1
---
# Reuse carve occupancy labels and bound repeated empty-hole scans

## Acceptance Criteria

Mark empty/carved pixels as blocked once, avoid material lookups for every flood-fill neighbor, and restrict repeated brush hit checks to the transformed brush region. Preserve eight-neighbor connectivity, fragment ordering, mass, shape and custom RGBA. GPU work counters must fail before change; graphical total frame target remains unchanged.


## Notes

**2026-10-24T08:23:59Z**

RED GPU probe:323711 repeated seed/neighbor material queries and36864 pixel inspections on each missed repeat, including rotation. GREEN reuses blocked labels and transforms the brush rectangle:0 redundant material queries,110 repeat inspections and49 rotated inspections. Both cases preserve all36051 remaining pixel coordinates/materials, exact authored RGBA, mass and clean invalidation;2/2 under actual synchronization validation. Normal brush regression suite and displayed measurement next.
