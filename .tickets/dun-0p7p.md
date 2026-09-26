---
id: dun-0p7p
status: open
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

