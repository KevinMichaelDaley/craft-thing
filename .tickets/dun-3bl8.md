---
id: dun-3bl8
status: closed
deps: [dun-jo5i]
links: []
created: 2026-09-26T06:45:10Z
type: task
priority: 2
assignee: kmd
parent: dun-9e5b
tags: [gpu, fluid, projection]
---
# Project Eulerian face velocity with GPU pressure solve

Add MAC face velocities, gravity prediction, liquid pressure Poisson solve, and projected velocity used by conservative volume transport. Preserve resident chunk boundaries and zero-normal solid walls.

## Acceptance Criteria

GPU divergence diagnostic drops after projection in a closed liquid scene; free-surface water still falls; closed basin mass remains exact; chunk seam agrees with unsplit grid; one-second screenshot smoke passes.


## Notes

**2026-09-26T06:57:35Z**

GPU red-black SOR projection (20 sweeps), conservative face-velocity transport, resident seam equality, and one-second screenshot smoke pass. Live frame records simulation plus render in one submission and performs no per-frame physics readback. Closed 10x10 liquid diagnostic max divergence 0.019813; large-region convergence and native-resolution performance remain to be measured in dun-2b08.
