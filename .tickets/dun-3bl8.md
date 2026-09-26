---
id: dun-3bl8
status: in_progress
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

