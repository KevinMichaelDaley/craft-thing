---
id: ct-i5a4
status: closed
deps: [ct-touk]
links: []
created: 2026-10-11T05:44:08Z
type: task
priority: 1
parent: ct-fx1b
tags: [gpu, structural]
---
# GPU structural placement, bonded topology and terrain anchors

Add explicit place/mark structural APIs, stable structural/member/joint IDs, immutable rest geometry and collision correspondence. Construct same-material and mixed-material graphs from validated attachment footprints, not proximity alone. Use GPU-owned storage within the descriptor budget. Ordinary terrain remains the anchor; no automatic world-wide promotion.

## Acceptance Criteria

Graph edges and anchor constraints distinguish bonded, bearing, pinned and moment-resisting joints. Invalid marking/placement is atomic. Cross-chunk joins have one owner; cycles do not create extra strength; unresident neighbors yield UNKNOWN, not empty anchors. Structural colliders support rigid bodies without manual kinematic flags; no normal-frame graph readback. Capacity exhaustion preserves the prior graph and geometry.


## Notes

**2026-10-11T06:45:36Z**

GPU authoring, conservative source transfer, immutable IDs/generations, finite typed anchors/joints, exact world-slot residency and topology caching are implemented. Ownership guards include body motion/spawn/remove, automatic solver enable and body-only save/load rejection until ct-1n9n graph snapshots. Eleven placement tests pass normally and at 640x448/70 slots; 164 headless cases pass with Vulkan validation. Full existing quarter-native and UI suites passed without Wayland stalls. 16-member warm quarter diagnostic: rigid/topology 4.545 ms, whole tick 14.303 ms, mapped/device 64.407/107.196 MiB. Rooted graphs deliberately remain UNKNOWN until ct-il9b mechanical equilibrium; no runtime CPU graph solve/readback.
