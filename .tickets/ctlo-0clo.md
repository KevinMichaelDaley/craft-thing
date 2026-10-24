---
id: ctlo-0clo
status: in_progress
deps: []
links: []
created: 2026-10-24T11:14:02Z
type: task
priority: 1
---
# Bound material brush dispatches to the edited region

## Acceptance Criteria

GPU regression first proves material brushing redraws unrelated display pixels and launches over the whole grid. Preserve untouched image/atlas pixels, update every cell in clipped circular brush across chunk edges, dispatch only brush bounds. Keep rigid-body carving and synchronous API semantics.


## Notes

**2026-10-24T11:22:53Z**

RED actual Vulkan: radius3 changes8163 unrelated image pixels and launches32 workgroups on128x64. GREEN actual core+synchronization validation checks circular edits across a chunk seam and both viewport corners: one workgroup, zero unrelated image changes, all selected atlas materials correct. Body-carve regression12/12 passes including colored concave bodies, rotation, repeated splits, leaves and structural members. Full displayed measurement running.
