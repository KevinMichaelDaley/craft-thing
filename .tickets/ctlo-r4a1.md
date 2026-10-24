---
id: ctlo-r4a1
status: in_progress
deps: []
links: []
created: 2026-10-24T06:56:28Z
type: task
priority: 1
---
# Fuse independent per-node MPM transfers and water feedback

## Acceptance Criteria

Fuse particle-to-grid, grid solve and same-node water feedback without changing two physical substeps, materials, particle records, grid forces or velocities. Bytewise GPU oracle covers wet/dry, two particle layers, obstacles and missing pages. Validate displayed frame time.


## Notes

**2026-10-24T07:02:06Z**

Fused node p2g, grid solve and same-node water feedback are bytewise exact in four wet/dry, multilayer, obstacle and missing-page fixtures with actual synchronization validation. DC_MPM_FUSED_GRID=1 remains opt-in pending performance evidence. Ordinary500-frame Wayland run32.760ms mean /39.435ms worst; four paused captures identical. Target still unmet.
