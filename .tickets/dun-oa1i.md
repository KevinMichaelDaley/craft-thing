---
id: dun-oa1i
status: open
deps: []
links: []
created: 2026-09-26T05:25:20Z
type: epic
priority: 1
assignee: kmd
tags: [world, streaming]
---
# Infinite-canvas chunk representation and threaded streaming

Implement 64x64 cell chunks with signed 64-bit world coordinates, bounded GPU residency, persisted dirty chunks, procedural defaults, and a separate CPU worker thread for generation/load/save. Simulation uses only resident slots.

## Acceptance Criteria

Camera traverses an unbounded coordinate space; chunks load and evict within a fixed memory budget; modified material and fluid state survives eviction/reload; worker thread never mutates Vulkan resources; cross-chunk mass and particles are preserved.

