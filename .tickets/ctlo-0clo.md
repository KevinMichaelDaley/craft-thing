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

