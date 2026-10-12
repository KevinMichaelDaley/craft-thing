---
id: ct-1n9n
status: in_progress
deps: [ct-0iq3]
links: []
created: 2026-10-11T05:44:09Z
type: task
priority: 1
parent: ct-fx1b
tags: [gpu, structural]
---
# Persist structural graph identity, damage and conversion state across streaming

Persist structure/member/joint IDs, world anchors and footprint cells, rest geometry, attachment types, material state, grain orientation, damage, grace timers and output transaction identity. Derive transient matrices/labels on reload; checkpoint dirty graph mutations in the world save transaction.

## Acceptance Criteria

Camera rebases leave forces, stress and countdowns invariant. Cold neighbor constraints remain UNKNOWN until loaded. Save/reopen mid-overload and across a cut preserves exact authoritative state; no duplicated descendants, lost joints or reset timers. Page-slot reuse cannot anchor to unrelated terrain. Saturated pending conversions survive restart and later commit exactly once. Older worlds without structural records load unchanged.


## Notes

**2026-10-12T16:43:21Z**

Complete GPU footprint residency now detects cold diagonal corner pages even with resident endpoints. The regression freezes100 UNKNOWN ticks, then resumes original10/90 grace count and converts exactly at90 confirmed ticks. Normal/quarter case passes; full headless212/212. Snapshot/identity remapping and world-save integration remain unfinished.

**2026-10-12T16:55:39Z**

Five streaming/identity regressions pass normal and quarter-native; full headless216/216. GPU author transactions remap member/interface damage and confirmed-loss history by ID+generation, preserve history across reorder/unchanged republish, reset fresh member generations, reject stale generations. Component-node clocks preserve history across topology changes; UNKNOWN remains frozen. File snapshots, atomic world checkpoint integration and converted/pending reload acceptance remain unfinished.
