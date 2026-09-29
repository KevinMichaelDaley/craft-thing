---
id: dun-046t
status: closed
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


## Notes

**2026-09-29T06:52:34Z**

Offscreen GPU workspaces retain 0.92 face velocity per 60 Hz world tick, raised to elapsed-tick power in the fluid correction pass. Visible default remains 0.997. GPU regression verifies stronger equal-time damping and exact water mass; full make test, test_ui, test_half_native pass. Deep offscreen water still sends 61.16 cell-volumes into destination, 54.28 at least eight cells inward after 120 ticks. Half-native isolated benchmark 62.17 ticks/s, 2.4 ms fluid GPU/tick. RED/GREEN/REFACTOR commits 4e480fd, bf440c7, current.
