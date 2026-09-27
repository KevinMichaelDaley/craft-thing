---
id: dun-yji8
status: in_progress
deps: []
links: []
created: 2026-09-27T23:30:05Z
type: task
priority: 2
assignee: kmd
parent: dun-9e5b
---
# Settle residual water and grain bouncing in the interactive demo

The always-on test spring feeds the center basin, and water plus grains currently retain 99.9% velocity per solve. Increase damping modestly while preserving the fast lateral water tests and elapsed-time-scaled fluid behavior.

## Acceptance Criteria

Projected wet-face speed decays measurably more than current 0.999 factor, free sand loses slightly more horizontal speed, mass and lateral-flow tests pass, README explains the intentional spring and F toggle.

