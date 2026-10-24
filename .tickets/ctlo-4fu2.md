---
id: ctlo-4fu2
status: in_progress
deps: []
links: []
created: 2026-10-24T06:46:01Z
type: task
priority: 1
---
# Cache exact pressure stencil metadata across SOR sweeps

## Acceptance Criteria

Cache terrain, face coefficients and divergence per pressure batch; retain every red/black sweep, refresh after between-batch edits, and match dense reference pressure/velocity bytes. Validate displayed full-frame timing and memory budget.

