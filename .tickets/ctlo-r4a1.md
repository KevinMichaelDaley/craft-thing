---
id: ctlo-r4a1
status: closed
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

**2026-11-28T05:17:39Z**

Validated the shipping fused node transfer against separate dispatches on the RTX A2000. Four core/synchronization-validated configurations preserve every compared particle, grid, force, velocity and water-feedback byte, including two layers, wet/dry cells, obstacles and missing pages. The unchanged 640x360 displayed dense water/grains workload ran 180 frames at two fixed 60 Hz steps per frame, cadence 6: split mean/p95 6.104/9.865 ms; default fused 9.850/15.804 ms and repeat 9.549/16.327 ms. All three frame checks passed. Other work shared the machine, and these results do not establish a fusion speedup.
