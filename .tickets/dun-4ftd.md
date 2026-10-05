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


## Notes

**2026-10-05T02:16:41Z**

Prerequisites dun-0p7p and dun-rwls are complete. GPU records in gpu_internal.h are 80 bytes: cached body/current and previous viewport pose, signed world chunk word pairs, canonical local pose, visible flag. rigid.comp contains checked relative_chunk/shifted_chunk helpers; pool ownership is independent of terrain atlas slots. Current and conservative swept masks occupy separate halves of binding 3; normal physics does not read transforms back. World origin uses 32-byte push constants for rigid; future broadphase should consume world anchors and preserve explicit sleeping/cold-page boundaries.
