---
id: dun-046t
status: in_progress
deps: []
links: []
created: 2026-09-29T06:43:19Z
type: bug
priority: 1
assignee: kmd
parent: dun-0t9m
tags: [gpu, fluid, offscreen]
---
# Damp offscreen Eulerian water without losing mass or seam transport

Large water volumes oscillate and bounce after leaving the visible map. Give offscreen GPU fluid velocity stronger timestep-scaled damping while preserving conservative Eulerian mass transport and cross-workspace flow.

## Acceptance Criteria

Offscreen damping is applied on GPU with no per-frame copyback; a regression proves velocity decays faster offscreen than in visible simulation at equal elapsed time, mass is conserved, and deep water still crosses the offscreen seam. Full Make tests and interactive demo pass.

