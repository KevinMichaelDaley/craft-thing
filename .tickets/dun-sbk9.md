---
id: dun-sbk9
status: open
deps: [dun-9qub, dun-c459]
links: []
created: 2026-09-26T07:43:36Z
type: task
priority: 2
assignee: kmd
parent: dun-i1zd
tags: [gpu, wood, rigid, fluid]
---
# Apply GPU buoyancy and drag to wood bodies

Wood rigid bodies float and turn in water based on density and submerged shape.

## Design

Sample Eulerian water volume against rasterized convex occupancy, compute displaced-volume buoyancy and velocity-relative drag on GPU, and feed force/torque into rigid integration. Couple to fluid displacement with equal-volume accounting; handle partially submerged and moving-water cases.

## Acceptance Criteria

Dry wood falls; low-density wood floats at a repeatable draft and follows flow; denser wood sinks. A water-level change changes force on the next tick, while water volume remains accounted for.

