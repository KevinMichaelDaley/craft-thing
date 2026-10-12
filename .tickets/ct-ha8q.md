---
id: ct-ha8q
status: in_progress
deps: [ct-gf6q, ct-1n9n, dun-c459, dun-08qq, dun-sbk9, dun-spbq]
links: [dun-c459, dun-sbk9, dun-08qq]
created: 2026-10-11T05:44:09Z
type: task
priority: 1
parent: ct-fx1b
tags: [gpu, structural]
---
# Quarter-native structural visual battery, buoyancy handoff and budgets

Integrate marked structural controls, load/stress/failure visualization and paced Vulkan scenes into test_ui and test_quarter_native. Complete submerged-structure load sampling and post-collapse handoff to existing material-derived buoyancy/drag; link existing water and extraction tickets rather than duplicating sink/float rules.

## Acceptance Criteria

Visual scenes show two concave rigid bodies held by a sideways beam with an angled brace; removing the brace concentrates stress then causes delayed material-specific failure and both bodies fall. Also show symmetric/eccentric bridge loads, wood splitting vs stone cracking vs crumbling, submerged wood draft/stone sinking derived from composition, and chunk-boundary save/reload. Density is never a manual sink/float toggle. Water/material conservation, deterministic timing, stable resting frames, Vulkan validation, timestamps and bounded storage are checked at quarter-native resolution; existing headless/UI suites pass.


## Notes

**2026-10-12T21:47:16Z**

Compiling RED for actual quarter-native L/U concave loads on a sideways beam plus angled brace: require GPU stress display, intact support, stress rise after brace removal, delayed cascade and visible BMP artifacts. Initial RED fails missing structural overlay, before scene placement; remaining dependencies and visual acceptance stay incomplete.
