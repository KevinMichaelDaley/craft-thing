---
id: dun-0p7p
status: in_progress
deps: [dun-o1go, dun-d8gb]
links: []
created: 2026-09-26T05:25:20Z
type: task
priority: 2
assignee: kmd
parent: dun-mi1e
tags: [gpu, rigid]
---
# Integrate GPU bodies and rasterize occupancy

Store stable body IDs and transforms in GPU buffers, integrate fixed-step motion, rasterize current/swept occupancy into resident chunks.

## Acceptance Criteria

A falling body moves predictably and occupancy follows its transform across chunk edges.


## Notes

**2026-09-26T06:08:47Z**

Prototype GREEN: one GPU-owned box integrates at fixed 60 Hz, crosses resident chunk edge, settles on terrain, and rasterizes current viewport occupancy. Headless and streamed-window end-to-end tests pass with validation. Remaining acceptance: swept occupancy and multiple stable body IDs; world-space persistence tracked by dun-rwls.
