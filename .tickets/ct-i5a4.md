---
id: ct-i5a4
status: open
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

