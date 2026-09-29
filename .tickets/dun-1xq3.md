---
id: dun-1xq3
status: in_progress
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

