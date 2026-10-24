---
id: ctlo-qnrt
status: open
deps: []
links: []
created: 2026-10-24T07:36:04Z
type: task
priority: 1
---
# Reuse carve occupancy labels and bound repeated empty-hole scans

## Acceptance Criteria

Mark empty/carved pixels as blocked once, avoid material lookups for every flood-fill neighbor, and restrict repeated brush hit checks to the transformed brush region. Preserve eight-neighbor connectivity, fragment ordering, mass, shape and custom RGBA. GPU work counters must fail before change; graphical total frame target remains unchanged.

