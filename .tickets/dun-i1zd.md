---
id: dun-i1zd
status: open
deps: []
links: [dun-mi1e, dun-9e5b]
created: 2026-09-26T07:42:49Z
type: epic
priority: 1
assignee: kmd
tags: [gpu, wood, rigid, fluid]
---
# GPU wood buoyancy and long-duration rot

Wood exists as rigid bodies, floats or sinks according to displaced water and density, and slowly rots when partly immersed for months of simulation time.

## Design

The rigid pass samples the previous completed Eulerian water state to accumulate buoyancy and drag; the following fluid pass sees new rigid occupancy. Wood body state holds material density and fixed-point wet exposure/rot state. Use an accelerated deterministic clock in tests, with normal fixed-tick scaling and chunk/world-space persistence.

## Acceptance Criteria

A wood body floats, responds to changing water level, and remains stable at chunk boundaries; partial immersion for simulated months changes rot state and physical properties, while dry or fully submerged controls follow the specified exposure rule. No per-frame CPU physics readback.
