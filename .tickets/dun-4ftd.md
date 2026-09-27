---
id: dun-4ftd
status: open
deps: [dun-0p7p, dun-rwls]
links: [dun-spbq]
created: 2026-09-26T07:43:35Z
type: task
priority: 2
assignee: kmd
parent: dun-5kye
tags: [gpu, rigid, broadphase, streaming]
---
# Build world-space GPU AABB broadphase

Generate candidate body-body and body-terrain pairs for all active rigid bodies.

## Design

Bin swept AABBs into chunk/cell buckets on GPU with stable pair IDs, bounded storage, deduplication, and explicit overflow detection. Include neighboring resident chunks; an absent page is a blocking boundary until streaming resolves it.

## Acceptance Criteria

Dense piles and fast supported motions generate every true overlap without duplicate pairs; cold camera movement and slot reuse do not change pair ownership; GPU counters/timings are opt-in diagnostics only.

