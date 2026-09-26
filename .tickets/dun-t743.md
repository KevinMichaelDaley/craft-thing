---
id: dun-t743
status: open
deps: [dun-d8gb]
links: []
created: 2026-09-26T05:25:20Z
type: task
priority: 2
assignee: kmd
parent: dun-oa1i
tags: [world, procgen]
---
# Generate deterministic procedural chunks from seed and coordinates

Implement CPU procedural chunk generation using world seed plus chunk coordinates, with terrain, caves, and test basins spanning chunk boundaries.

## Acceptance Criteria

Generation is order-independent; same seed and coordinates reproduce exact cells; adjacent chunks meet without seams.

