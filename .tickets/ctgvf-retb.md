---
id: ctgvf-retb
status: open
deps: []
links: [ct-8vqy, ctsg-6060]
created: 2026-11-26T19:28:15Z
type: bug
priority: 2
assignee: kmd
tags: [gas, physics, vulkan]
---
# Restore unbiased mean momentum in the subgrid smoke fixture

The 9513f4f3 displayed publication suite fails test_native_subgrid_smoke_gains_detail_without_stock_or_mean_impulse. Reference momentum was (1.43702467, 2.74127378), and subgrid momentum was (1.43771857, 2.73811773); the gate is 1e-4 per axis. Energy and detail checks reported 3870.33769/3870.33768 and 0.108987309/0.110965142.

## Acceptance Criteria

Reproduce in isolation, trace the net perturbation and mean force, then preserve the existing momentum, stock, energy, and visible-detail bounds.
