---
id: ctsct-f96g
status: in_progress
deps: []
links: []
created: 2026-11-27T00:52:55Z
type: task
priority: 1
assignee: kmd
parent: ctfm-w80b
tags: [world-clock, streaming]
---
# Persist completed simulation time with streamed chunk data

The existing chunk worker restores physical stock but has no common timestamp for offscreen promotion. Carry a known completed world time with the immutable chunk packet so the offscreen scheduler can later compare saved and current world time.

## Acceptance Criteria

Round-trip a finite nonnegative completion time through the production stream save and load path, including a known zero, the optional world-state channels and staged saves. Preserve legacy archives and distinguish an absent timestamp. Reject corrupt or invalid clock input without replacing the previous snapshot.

