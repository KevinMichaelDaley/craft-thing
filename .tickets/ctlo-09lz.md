---
id: ctlo-09lz
status: in_progress
deps: []
links: []
created: 2026-10-24T11:08:54Z
type: task
priority: 1
---
# Reuse terrain contact presence across rigid substeps

## Acceptance Criteria

Build conservative per-chunk terrain and grain presence on GPU once per rigid frame, refresh for standalone contact calls and edits, preserve exact manifolds including both grain layers; do not rescan entire terrain chunks for every pair in every substep. No capacity limit or CPU scan.

