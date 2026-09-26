---
id: dun-rwls
status: open
deps: []
links: []
created: 2026-09-26T06:08:36Z
type: task
priority: 2
assignee: kmd
parent: dun-mi1e
tags: [gpu, rigid, streaming]
---
# Keep rigid bodies in world coordinates across chunk streaming

Current prototype stores one viewport-local body; camera panning changes the terrain beneath it and body state is not streamed. Add world-space body ownership, resident-chunk transfer, save/load, and visibility clipping.

## Acceptance Criteria

A spawned body remains at the same world cell after panning away and back; body state survives resident chunk eviction and reload; end-to-end window smoke verifies both.

