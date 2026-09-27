---
id: dun-yji8
status: closed
deps: []
links: []
created: 2026-09-27T23:30:05Z
type: task
priority: 2
assignee: kmd
parent: dun-9e5b
---
# Settle residual water bouncing in the interactive demo

The always-on test spring feeds the center basin, and water retains 99.9% velocity per unit solve time. Increase water damping modestly while preserving fast lateral flow and elapsed-time scaling. Grain damping remains at 99.9% because stronger global grain damping disrupted free fall and water-grain momentum exchange.

## Acceptance Criteria

Projected wet-face speed decays measurably more than the current 0.999 factor; mass, lateral-flow, and water-grain momentum tests pass; README explains the intentional spring and F toggle.

## Notes

**2026-09-27T23:34:43Z**

Water velocity factor 0.997 per unit solve time gives projected face speed 0.987030 after one step versus 0.989010 before. Full headless suite, UI suite, and half-native smoke passed; half adaptive sample 70.9 fps. Global grain damping was left at 0.999 because stronger factors broke falling pile and water-grain momentum tests.
