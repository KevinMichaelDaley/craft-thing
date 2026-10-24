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

