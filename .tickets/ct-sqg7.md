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


## Notes

**2026-10-17T05:43:59Z**

Targeted GREEN: GPU presence planner once per candidate,16cooperative workgroups partition exact pixel enumeration, atomic completion flags and parallel final skipped-count reduction. Typed transient words15..23 and work ABI2 reject stale binaries/shaders. Dispatch10/10,ABI7/7,recording2/2,terrainwork3/3,exact44contact/physical-plane/image comparison all pass; explicit synchronization validation clean. Full terrain presence remains64probes, not16duplicates. Sustained180 native640x360 physics/display passes twice:25.773/25.776ms mean,32.789/32.973ms p95,38.800/38.796FPS; cold91.454/91.018ms remains open. Full current headless/UI/quarter and downstream dynamics validation pending.

**2026-10-17T06:04:52Z**

Further audit only: typed terrain flag reads are currently atomic per lane to avoid racing atomic found-flag writes. A future shared broadcast could reduce this to one atomic read per workgroup without altering the flag protocol. Not implemented; do not treat this as a measured cause or completed optimization. The current full regression remains frozen on the proven 16-group implementation.

**2026-10-17T06:19:51Z**

Full headless acceptance passed: 395 reported passes, exit 0, with Vulkan validation and explicit synchronization validation enabled; zero VUIDs or synchronization hazards. All 24 runtime SPIR-V modules validate. The source guard now distinguishes unsafe shared-return predicates from safe lane-zero statistics reads, with positive and negative detector regressions. UI is running and quarter-native is queued; ticket remains in progress until those finish.
