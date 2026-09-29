---
id: dun-qr6s
status: closed
deps: []
links: []
created: 2026-09-29T05:52:05Z
type: bug
priority: 1
assignee: kmd
parent: dun-9det
tags: [mpm, mud, gpu]
---
# Keep flowing mud connected with nonlinear GPU MPM viscosity

Large wet dirt masses scatter into isolated particles during flow rather than sliding as a coherent viscoplastic sheet and filling small gaps.

## Acceptance Criteria

A wet dirt mound remains substantially connected after two seconds while spreading farther than dry dirt; low-shear mud has higher effective viscosity, high-shear mud yields and slides; GPU-only, conserved dirt and water; before/after scene demonstrates gaps closing.


## Notes

**2026-09-29T06:32:01Z**

GPU mud grid viscosity now couples neighboring cell-centered particles with symmetric shear-thinning forces; 120-tick regression keeps 61/64 in largest body, conserves dirt and water, and spreads farther than dry dirt. Full make test, test_ui, test_half_native pass; UI smoke 62 mud at base, 54 at half-native. RED/GREEN/REFACTOR commits f1a885f, 9cf3a23, cb5d37f.
