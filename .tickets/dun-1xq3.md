---
id: dun-1xq3
status: closed
deps: []
links: []
created: 2026-09-29T07:00:13Z
type: bug
priority: 1
assignee: kmd
parent: dun-9e5b
tags: [fluid, gpu]
---
# Settle Eulerian water with GPU viscosity and no-slip solid walls

Water remains perpetually bouncy in resident and offscreen basins. Apply globally higher viscous momentum diffusion before pressure projection and a no-slip ghost condition at solid faces, while retaining conservative water transport and current offscreen damping.

## Acceptance Criteria

A disturbed enclosed basin loses kinetic energy, wall-adjacent tangential velocity decays more than interior velocity, mass is exact, cross-chunk spreading persists, and half-native tick performance is measured. All work remains GPU-resident.


## Notes

**2026-09-29T07:26:01Z**

GPU predictor now applies race-free four-neighbor viscous diffusion (0.03 cell²/world tick) before pressure projection, with odd-reflection no-slip ghosts plus 0.05/tick solid-wall drag; missing workspace pages use zero shear. Wall-adjacent tangential velocity 0.8774 after one step versus interior 0.9870, then -0.0143 after one second; mass exact. Split deep basin 142.13 vs monolithic 145.38 cell-volumes within tolerance. Full make test, test_ui, test_half_native pass; half-native 61.45 ticks/s and 2.4 ms GPU fluid/tick. Commits f0a7876, 370e7e3, fe2ca93, current.
