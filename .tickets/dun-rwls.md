---
id: dun-rwls
status: in_progress
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

The GPU pool stores up to 64 stable-ID viewport-local bodies (`dun-0p7p`); camera panning changes the terrain beneath them and body state is not streamed. Add world-space body ownership, resident-chunk transfer, save/load, and visibility clipping while preserving IDs, independent motion, and occupancy masks.

## Acceptance Criteria

A spawned body remains at the same world cell after panning away and back; body state survives resident chunk eviction and reload; end-to-end window smoke verifies both.
