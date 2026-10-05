---
id: dun-rwls
status: closed
deps: []
links: []
created: 2026-09-26T06:08:36Z
type: task
priority: 2
assignee: kmd
parent: dun-mi1e
tags: [gpu, rigid, streaming]
---
# Keep rigid bodies in world coordinates across chunk streaming

The GPU pool stores up to 64 stable-ID viewport-local bodies (`dun-0p7p`); camera panning changes the terrain beneath them and body state is not streamed. Add world-space body ownership, resident-chunk transfer, save/load, and visibility clipping while preserving IDs, independent motion, and occupancy masks.

## Acceptance Criteria

A spawned body remains at the same world cell after panning away and back; body state survives resident chunk eviction and reload; end-to-end window smoke verifies both.

## Notes

**2026-10-05T02:16:41Z**

Completed RED-GREEN-REFACTOR: GPU bodies now own signed 64-bit chunk anchors plus canonical local fixed-point poses. Checked 32-bit word arithmetic carries transforms across seams without shaderInt64. Camera rebasing and resident clipping refresh masks without advancing time; offscreen bodies sleep independently of terrain atlas slot reuse. Atomic versioned rigid_bodies.bin snapshots restore IDs, poses, sizes and velocities across world reopen, with validated load before pool replacement. Three headless world regressions and quarter-native eviction/reload window regression pass. Final make test: 100 passed; quarter-native: 35 tests plus viewport smoke passed (viewport required retry); make test_ui: 5 controls tests and all 10 scenes passed, Vulkan validation clean. Intel Iris Xe dense performance gate: 16.442 ms physics (60.82 Hz), render-inclusive 57.65 Hz. Intermittent Wayland presentation wait tracked separately in ct-d50u. Bounded world pool is 64 bodies including sleepers; broadphase/XPBD remains dun-4ftd/dun-x9ei.
