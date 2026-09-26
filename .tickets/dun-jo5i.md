---
id: dun-jo5i
status: closed
deps: [dun-o1go, dun-cwhn]
links: []
created: 2026-09-26T05:25:21Z
type: task
priority: 2
assignee: kmd
parent: dun-9e5b
tags: [gpu, fluid]
---
# Implement conservative Eulerian cell flux

Compute directional fluxes between fixed cells, resolve source/destination capacity, and update ping-pong fluid mass layers at a stable substep size.

## Acceptance Criteria

Closed basin retains total mass within fixed-point tolerance over long runs; fluid respects solid barriers and crosses resident chunk edges.

