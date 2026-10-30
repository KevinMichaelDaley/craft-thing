---
id: ct-raqq
status: in_progress
deps: []
links: []
created: 2026-10-30T19:12:03Z
type: task
priority: 1
assignee: kmd
parent: ctfs-5gil
tags: [performance, save]
---
# Reduce bonfire shutdown save latency

Half-native one-frame bonfire reaches gas enabled at 3.074s but process wall time is 28.17s. Find dominant phase in final structure/chunk/gas checkpoint save and reduce the 25s exit cost while preserving persisted state and no dirty chunks.

## Acceptance Criteria

Profile stage timings; reduce one-frame bonfire exit wall time materially with identical persisted state; shutdown and full regression tests pass; changes pushed.


## Notes

**2026-10-30T19:14:46Z**

Startup trace one-frame half-native: gas enabled3.015s, wall28.97s. DC_SAVE_TRACE: final body structure5.828s, schedule_saves repeats body structure9.247s, chunk export/queue8.857s, total flush23.947s. RED opt-in test catches repeated structure checkpoint. The same structure is saved twice during destroy when dirty chunks are queued.
