---
id: ctsct-a986
status: in_progress
deps: [ctlrcc-1nhb, ctsct-efcn]
links: []
created: 2026-11-27T01:19:42Z
type: task
priority: 1
assignee: kmd
parent: ctfm-w80b
tags: [world-clock, streaming]
---
# Initialize and recover the shared world simulation clock

Chunk timestamps now survive stream and embedded checkpoint recovery. The level preserves known times, while new and legacy chunks currently enter without a known absolute time. Establish the session clock used for newly generated chunks and future offscreen catch-up.

## Acceptance Criteria

Advance a shared elapsed time only for confirmed world ticks. Recover its epoch across a staged world save and stamp newly generated chunks at admission. Keep legacy persisted chunks visibly unknown until an explicit migration policy can establish their time. Verify paused/loading frames, retries and cold return without catch-up duplication.


## Notes

**2026-11-27T01:19:42Z**

The per-chunk stream and DCS timestamps are already bound to their physical source. Connect this shared clock to the foreground owner before scheduling asynchronous offscreen work.
