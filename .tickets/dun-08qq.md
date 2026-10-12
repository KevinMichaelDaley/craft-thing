---
id: dun-08qq
status: in_progress
deps: [dun-9qub, dun-c459]
links: [dun-spbq, ct-ha8q]
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


## Notes

**2026-10-12T20:56:43Z**

Stone and density1.2 wood sink and settle on terrain in GPU tests while preserving material mass and every water unit. Buoyancy uses exact submerged shape volume and density-derived mass. Normal/quarter14-case water suites and full246-case headless battery pass. Automatic extraction and final visible/streaming integration remain with dun-spbq/ct-ha8q.
