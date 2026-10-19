---
id: ctr-9k5m
status: in_progress
deps: [ctr-9svu]
links: []
created: 2026-10-14T03:12:55Z
type: task
priority: 1
parent: ctr-6fii
tags: [renderer, vulkan, appearance, lighting]
---
# Build conservative GPU occlusion and transmission hierarchy

Build renderer-only opacity/transmission from exact terrain and current rigid/structural coverage, including concavities and thin walls. Parallel hierarchy generation, ownership-aware dirty masks and streaming/known-sky boundary contract.

## Acceptance Criteria

One-pixel walls block, holes pass light, rotated bodies update, unknown pages cannot leak light, albedo brightness does not change shadows. No global single-invocation scene scan; unchanged tiles skip work.


## Notes

**2026-10-14T04:28:32Z**

GPU hierarchy and scene optical-channel generation validated; odd dimensions, thin walls and unknown residency block conservatively. Frame integration uses exact body overlap and material coverage independently of color. Paused scene skips hierarchy/lighting via GPU dirty masks and indirect dispatch. Moving/rotating concave display cases and measured costs remain pending.
