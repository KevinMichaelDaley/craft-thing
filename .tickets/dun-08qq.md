---
id: dun-08qq
status: open
deps: [dun-9qub, dun-c459]
links: [dun-spbq]
created: 2026-09-26T07:43:18Z
type: task
priority: 2
assignee: kmd
parent: dun-9det
tags: [gpu, rigid, fluid, stone]
---
# Make falling stone a sinking rigid body

Detached or dropped stone becomes a convex rigid body that displaces water and sinks.

## Design

Transfer stone cell mass into one or more stable-ID convex bodies on GPU; use final rigid occupancy for fluid displacement and buoyancy force derived from submerged volume. Return debris to the material system only through explicit conversion.

## Acceptance Criteria

A dropped stone sinks through water and settles on terrain without lost water or stone mass; crossings and saves preserve body state, and the visible scene has no per-frame copyback.

