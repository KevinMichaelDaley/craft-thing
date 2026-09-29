---
id: dun-qr6s
status: in_progress
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

