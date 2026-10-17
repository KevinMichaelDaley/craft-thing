---
id: ct-sqg7
status: in_progress
deps: [ct-k6ym]
links: [ct-tu55]
created: 2026-10-17T05:30:50Z
type: task
priority: 1
tags: [vulkan, physics, performance, parallel]
---
# Distribute exact pixel contact candidates across workgroups

Displayed sustained native360 frame72 has ~21.6ms contact work with0emitted contacts. Frozen candidate profiling identifies terrain chunk(0,2) query3328pixels as dominant; one64-lane workgroup handles each whole candidate. Terrain early exit is bounded and validated but has only small whole-frame effect.

## Acceptance Criteria

Parallelize exact pixel contact enumeration across multiple cooperative workgroups per candidate, with GPU-only deterministic dispatch planning and transactional-safe contact publication. Preserve every pixel/hole/material, surface rejection, mixed legacy fallback semantics, exact constraint features, overflow and skipped-candidate counts. Keep bounded dispatch dimensions and canonical inactive triplets; reject incompatible shader work ABI. RED recording/dispatch/ABI tests compile/fail; GREEN exact44-contact/image comparison and GPU scaling, full validated headless/UI/quarter-native checks; native640x360 180-frame mean/p95 gates unchanged. No CPU solve/readback fallback or quality reduction.

