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


## Notes

**2026-10-24T18:16:47Z**

GREEN actual core+synchronization validation:15 coupled frames match all authoritative chunk bytes against copy-based reference through colored leaf debris, complete air deletion, dirt restamping, empty reload and seeded reload. Direct metadata-only commit probe clears output/counts while preserving every input particle byte, proving the copy is absent. Real displayed500-frame repeated-brush replay24.525ms mean32.906ms worst vs28.158/34.860 before; four paused captures identical. No new particle pool, same two substeps. Frame gate remains RED.
