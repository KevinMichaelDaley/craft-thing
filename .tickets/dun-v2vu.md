---
id: dun-v2vu
status: open
deps: []
links: []
created: 2026-09-26T05:25:20Z
type: epic
priority: 1
assignee: kmd
tags: [level, procgen, ui]
---
# Interactive procedural level and material testbed

Create a reproducible procedural level using the same chunk representation and streaming path as the engine. Provide a simple renderer and interactive brush to place/erase materials and observe rigid, fluid, and sand behavior.

## Acceptance Criteria

A user can pan/zoom an infinite canvas, paint/erase selected materials, pause/step/reset, enter a seed, and see chunks load while moving; edits persist after leaving and returning; benchmark scenes report native-resolution timings.

