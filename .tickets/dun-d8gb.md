---
id: dun-d8gb
status: in_progress
deps: [dun-kjp9]
links: []
created: 2026-09-26T05:25:20Z
type: task
priority: 2
assignee: kmd
parent: dun-oa1i
tags: [world, data]
---
# Define chunk coordinates, cell layout, and residency states

Define signed 64-bit world coordinates, 64x64 chunk indexing, stable material IDs, GPU slot/page-table layout, and active/sleeping/loading/dirty state transitions.

## Acceptance Criteria

Negative coordinates and boundaries map correctly; CPU serialization and shader layouts agree; resident slot reuse cannot alias an in-flight chunk.

