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


## Notes

**2026-10-24T11:18:31Z**

GREEN actual core+synchronization validation: contact mechanics10/10, cached negative terrain work3/3 (per-pair scanned cells0), frozen scalar manifold oracle7/7 exact. CPU command recording3/3 and solver recording5/5 preserve all8x8 iterations. New per-chunk flags are rebuilt before each solver batch and standalone contact call; terrain/grain masses cannot change during its rigid substeps. Real displayed500-frame replay29.160ms mean36.357ms worst vs30.283/37.502 prior, stable four paused captures; target still RED.
