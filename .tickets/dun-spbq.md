---
id: dun-spbq
status: open
deps: [dun-v75v]
links: [dun-4ftd, dun-08qq]
created: 2026-09-27T02:16:24Z
type: task
priority: 2
assignee: kmd
parent: dun-mi1e
tags: [gpu, rigid, components]
---
# Extract connected stone bodies using GPU component labels

Reuse the GPU root-hooking and size/count buffers from dun-v75v to label falling stone across resident chunk seams, construct stable rigid body records, and preserve identity while chunks stream.

## Design

Use a material-selectable cell predicate and world-space identity mapping. Resolve merges and splits deterministically on GPU and feed extracted bodies into the existing AABB broadphase and XPBD pipeline without normal-frame copyback.

## Acceptance Criteria

A connected stone region spanning resident chunks becomes one rigid body, disconnected regions remain separate, streamed chunks preserve identity, and all labeling and body construction stay on GPU.

