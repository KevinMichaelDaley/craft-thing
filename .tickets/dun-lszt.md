---
id: dun-lszt
status: closed
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


## Notes

**2026-09-26T06:01:31Z**

Seeded streamed level with Vulkan material painting, chunk eviction/reload, and positive/negative camera traversal passes end-to-end smoke under validation.
