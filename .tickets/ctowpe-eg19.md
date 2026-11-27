---
id: ctowpe-eg19
status: in_progress
deps: []
links: []
created: 2026-11-27T11:41:08Z
type: task
priority: 1
assignee: kmd
parent: ctowpe-3271
tags: [offscreen, streaming, physics]
---
# Apply rigid hydrodynamic impulses across the paired owner boundary

## Design

The native rigid solver samples neighboring unoccupied liquid, including the opposite side of a body on a chunk edge. Pair the sampling and equal-and-opposite reaction with the same owner cohort as the bulk solve. Preserve both momentum components, retained stock, pressure aperture and body motion.

## Acceptance Criteria

A partial kinematic obstruction at x63 and a half-occupied sealed pocket match monolithic reverse oil/slime flow after both fractional and ordinary elapsed ticks. Actual rigid feedback and both owner momenta must agree. Reject stale owners before applying impulses.


## Notes

**2026-11-27T11:41:29Z**

Reproduced at elapsed tick 2 with a kinematic partial obstruction on the shared face. Resident horizontal oil momentum at atlas cell 1983 is -0.777648926 versus -0.664539695 in the monolithic native solver. Both layouts agree before the first rigid integration step. The native rigid_hydro.glsl neighborhood lookup stops at the local page boundary.
