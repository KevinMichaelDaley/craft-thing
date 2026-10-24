---
id: ctlo-3s4c
status: in_progress
deps: []
links: []
created: 2026-10-24T11:35:09Z
type: task
priority: 1
---
# Cache immutable rigid pixel boundaries once per solver batch

## Acceptance Criteria

GPU-parallel packed-word boundary cache, refreshed after cuts/extraction and for standalone contact queries. Preserve every geometry/material/color byte and exact canonical manifolds for rotated concave bodies; validate cache growth and shader synchronization. No body limit, no CPU scan, and retain full scene memory budgets.

