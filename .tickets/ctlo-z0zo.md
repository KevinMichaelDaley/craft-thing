---
id: ctlo-z0zo
status: in_progress
deps: []
links: [ctlo-jnt7]
created: 2026-10-24T03:41:51Z
type: task
priority: 1
---
# Limit unlit palette updates to visible pixels

## Acceptance Criteria

Albedo-frame GPU work scales with the visible viewport rather than resident atlas capacity. Hidden edits retain fresh optical queries and become correctly textured immediately after panning. Lit mode still refreshes the complete lighting domain. Keep material colors, physical resolution and simulation halo unchanged; verify displayed frame times.


## Notes

**2026-10-24T04:00:15Z**

GREEN focused checks: both albedo-work cases pass, including hidden-edit opacity, correctly textured wood immediately after panning, bounded viewport dispatches, and body slot 65 not aliasing empty coverage. Scene renderer test also passes. Lit mode retains full atlas updates. Actual displayed workload measurement is pending.
