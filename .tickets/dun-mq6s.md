---
id: dun-mq6s
status: closed
deps: []
links: [dun-r1a0]
created: 2026-09-29T09:45:24Z
type: task
priority: 1
assignee: kmd
parent: dun-9e5b
tags: [fluid, gpu, pressure]
---
# Measure and bound GPU pressure residual in larger liquid domains

The current fixed-work Poisson solve is validated for a 32-cell-high river and adjacent workspace seam. Measure divergence and surface settling in much larger continuous liquid regions spanning several GPU workspaces at native resolution; improve the all-GPU solver if residuals or timing exceed the interactive budget.

## Acceptance Criteria

Automated multi-workspace large-basin regression compares divergence, hydrostatic surface, exact mass and GPU time to a monolithic reference. Any solver change has no per-frame CPU readback and keeps the half-native interactive tick target.


## Notes

**2026-09-30T03:36:14Z**

Three 64x64 GPU workspaces versus one 192x64 basin at 480 ticks: exact mass; seam level error 0.546 versus 0.086 cell; seam divergence 0.000 versus 3.808; fluid GPU 1.5 ms. Full make test passes. Half-native smoke: 62.2 presented FPS, 2.7 ms mean fluid GPU stage.
