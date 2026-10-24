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

