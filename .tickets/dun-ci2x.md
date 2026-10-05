---
id: dun-ci2x
status: in_progress
deps: [dun-4ftd, ct-l35t]
links: []
created: 2026-09-26T07:43:35Z
type: task
priority: 2
assignee: kmd
parent: dun-5kye
tags: [gpu, rigid, contacts]
---
# Generate convex-to-pixel GPU narrowphase contacts

Create terrain, MPM-material, and body-body contacts from convex pieces at cell resolution.

## Design

Use convex support/edge tests against occupied one-cell terrain and convex pairs from broadphase; emit contact normal, depth, feature IDs, material parameters, and world-space anchors into bounded GPU buffers. Avoid viewport-local coordinates and report overflow.

## Acceptance Criteria

Rotated convex stones and wood contact sloped and stepped terrain without missed one-cell obstacles; body-body contacts are symmetric and reproducible across chunk seams.


## Notes

**2026-10-05T02:50:07Z**

Broadphase prerequisite is complete. binding 4 now contains 64 80-byte body records followed by 8 header words, two membership words per (page_width+2)*(page_height+2) chunk bucket, and 4096 32-byte candidate records. Header is dc_gpu_broadphase_stats_t plus two reserved words. Candidate keys use sorted body IDs, or body ID + signed world chunk for terrain/boundary. Ordering is unspecified, deduplication is GPU-owned. Overflow flags 0x1 (capacity) or 0x2 (world coordinate range) make the set incomplete and must gate narrowphase/solving. Shared rigid_body.glsl provides ABI and checked signed chunk arithmetic. Data and counters remain GPU-owned unless explicit diagnostics are requested.

**2026-10-05T04:17:25Z**

Convex geometry prerequisite ct-l35t implements body-local pre-rotated polygons (3..8 fixed-point vertices, stone/wood metadata), Vulkan polygon-cell occupancy and version 2 persistence with legacy version 1 loading. Shared Body stride is now 152 bytes: original 80-byte prefix plus count/material and 8 ivec2 vertices; broadphase data starts after 64 such records. Candidate ABI remains unchanged. rigid_shape.glsl provides positive-area polygon-versus-cell separation tests. Narrowphase still must emit normal/depth/features/materials/world anchors, gate broadphase overflow, and cover terrain, MPM and body pairs. Custom polygons bypass the legacy box support approximation until contact response lands.
