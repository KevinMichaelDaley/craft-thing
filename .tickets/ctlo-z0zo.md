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

