---
id: ctlo-w232
status: in_progress
deps: []
links: []
created: 2026-10-24T04:52:07Z
type: task
priority: 1
---
# Batch Jacobi position iterations with workgroup synchronization

## Acceptance Criteria

Retain eight Jacobi iterations, unchanged correction arithmetic, coupled grain behavior and structural bond ordering. Batch passes use GPU workgroup synchronization and dynamically stride all rows and bodies without a total-body cap. Compare complete solver state against separate dispatches and measure actual displayed Vulkan frames before enabling.


## Notes

**2026-10-24T05:05:39Z**

Rejected as runtime optimization. Five exact body/contact cases (including1057 rows), two coupled grain cases and seven command-cache cases pass. Displayed fixed-clock comparison is45.239ms average /53.309ms worst versus~29.4ms for separate dispatches, with substantial cold pipeline compilation cost. Complete source and tests are preserved and pushed on experiment/workgroup-jacobi at b80a034; production worktree restores the existing multi-workgroup solver. Full-frame target remains unmet.
