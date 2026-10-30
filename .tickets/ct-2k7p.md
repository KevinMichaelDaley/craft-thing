---
id: ct-2k7p
status: open
deps: []
links: []
created: 2026-10-30T16:12:02Z
type: bug
priority: 1
assignee: kmd
parent: ctfs-5gil
tags: [stream, bonfire, performance]
---
# Finish bonfire benchmark without dirty-chunk save timeout

Half-native bonfire benchmark prints Timed out saving modified chunks during dc_level_view_destroy after the displayed timing result. All active chunks are marked dirty each step and shutdown attempts to save the entire resident set inside capacity*50ms+5s. Investigate save scheduling/dirty evidence and ensure finite benchmark/world closes with a successful flush without weakening persistence.

## Acceptance Criteria

RED test reproduces finite bonfire shutdown timeout; GREEN preserves gas, reactive, liquid and rigid checkpoints and completes save successfully within a bounded time; full relevant tests pass.

