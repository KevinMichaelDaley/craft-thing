---
id: dun-lszt
status: in_progress
deps: [dun-nycg, dun-t743, dun-f6oh]
links: []
created: 2026-09-26T05:25:21Z
type: task
priority: 2
assignee: kmd
parent: dun-v2vu
tags: [level, procgen]
---
# Build procedural playground and persistence test

Generate seeded terrain with slopes, caves, a fluid basin, and space for rigid bodies; save user edits in streamed chunk records.

## Acceptance Criteria

Same seed reproduces untouched terrain; edited chunks persist after eviction/reload; level remains usable while worker-thread streaming catches up.

