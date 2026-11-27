---
id: ctsct-efcn
status: in_progress
deps: [ctsct-f96g, ctolt-po5m, ctlrcc-1nhb]
links: []
created: 2026-11-27T00:59:33Z
type: task
priority: 1
assignee: kmd
parent: ctfm-w80b
tags: [world-clock, streaming]
---
# Propagate completed world time through level residency

The stream snapshot can carry a known chunk completion timestamp. Connect it to current level residency and world save recovery before using it for offscreen workspace admission.

## Acceptance Criteria

Stamp only confirmed physics work on the correct resident generation. Restore known chunk time across sleeping saves, cold return and reopen. Keep unknown legacy time explicit, and verify render retries, paused/loading frames and mixed tick durations do not duplicate or advance the wrong clock.


## Notes

**2026-11-27T00:59:33Z**

Use the immutable snapshot time in dc_stream_request_save_snapshot and the completed-tick reporting in the level pipeline. The offscreen scheduler and full display gates remain tracked in the parent.

**2026-11-27T01:07:03Z**

Host RED preserved the water cell while losing the timestamp. GREEN preserved 8.484375 s through a failed present, equal and mixed ticks, camera eviction, return and worker reopen. The fixture also checks stale worker generations, paused and loading frames, and a timed stream packet superseded by an untimed embedded structure snapshot. Twelve level, three chunk, eleven stream and four world-save cases passed. The application and native reactive stream test also built. Embedded structure time is still required for ordinary world reopen.
