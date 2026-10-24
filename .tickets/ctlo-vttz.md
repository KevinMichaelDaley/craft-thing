---
id: ctlo-vttz
status: in_progress
deps: []
links: []
created: 2026-10-24T12:06:08Z
type: task
priority: 1
---
# Swap granular particle buffers instead of copying after gather

## Acceptance Criteria

Alternate existing particle/output pools across the two unchanged MPM substeps; restore canonical pool binding at each frame boundary. Preserve exact authoritative particles, cells, velocities, colors and counts across edits, deletion, transfer, reload and extraction; no erased grains resurrect, no new particle pool or body cap. Actual core+synchronization validation and displayed full-frame benchmarking required.

