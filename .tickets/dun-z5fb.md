---
id: dun-z5fb
status: in_progress
deps: []
links: []
created: 2026-09-29T03:09:11Z
type: bug
priority: 0
assignee: kmd
---
# Let water activate cold offscreen chunks and spread beyond a stationary viewport

## Acceptance Criteria

A large water pour propagates beyond the resident GPU view without camera movement, continues across offscreen workspace seams toward equilibrium, and does not require per-frame GPU readback.

