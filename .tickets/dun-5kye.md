---
id: dun-5kye
status: open
deps: []
links: [dun-mi1e]
created: 2026-09-26T07:42:49Z
type: epic
priority: 1
assignee: kmd
tags: [gpu, rigid, xpbd]
---
# GPU multi-body broadphase, pixel contacts, and Jacobi XPBD

Replace the single-body contact prototype with GPU world-space rigid bodies using AABB broadphase, convex-to-pixel narrowphase, and parallel Jacobi XPBD constraints.

## Design

Broadphase emits bounded candidate pairs from world-space AABBs; narrowphase produces terrain and body contacts at one-cell resolution from convex pieces. Jacobi iterations accumulate per-body corrections separately and apply them simultaneously; velocities derive from corrected positions. Document conservative overflow behavior and deterministic replay scope. Rebuild final occupancy before water and MPM passes.

## Acceptance Criteria

Many bodies and terrain contacts solve on GPU with stable IDs, chunk crossings, no unsupported penetration at documented speed/substep bounds, and no normal-frame readback or CPU contact solve. Headless and interactive end-to-end tests cover pile-up, moving water, streaming, and stage timing.

