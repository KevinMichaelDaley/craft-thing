---
id: ct-wuci
status: open
deps: []
links: [dun-spbq]
created: 2026-10-12T23:07:46Z
type: task
priority: 2
tags: [gpu, rigid, extraction]
---
# Extend automatic extraction beyond the current exact body representation

Current GPU extraction conservatively retains roots above 256 cells, a 16-cell side, or four exact material rectangles. Extend the representation so larger and more complex isolated rigid-material components can move without geometry approximation or material/water loss.

## Acceptance Criteria

GPU extraction supports larger and more complex connected components with conserved geometry, composition, mass and momentum, material-derived buoyancy, transactional saturation, stable streamed ownership and measured quarter-native budgets. No runtime CPU physics fallback.

