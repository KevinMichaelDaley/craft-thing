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

