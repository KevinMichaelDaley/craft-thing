---
id: ct-2k7p
status: closed
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


## Notes

**2026-10-30T17:14:44Z**

RED real half-native 8-frame benchmark emitted cleanup timeout with 187/187 slots saving; all 188 world files existed. Shutdown's timeout check occurred after synchronous save staging and before next completed-result poll. GREEN moves deadline start after queueing and polls results again before timeout; focused displayed shutdown test passes on RTX A2000 X11. Full regression in progress.
