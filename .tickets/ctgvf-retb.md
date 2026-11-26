---
id: ctgvf-retb
status: in_progress
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

## Notes

**2026-11-26T19:58:16Z**

On b2710a77 the isolated subgrid check passed, with momentum changes -0.000003596 and -0.000032560. It still failed inside the complete b1ea20ab publication run. Compare fixture environment and prior-case option restoration before changing the subgrid solver.

**2026-11-26T20:35:23Z**

The new native GLSL impulse fixture reproduces a masked curl imbalance independent of pressure and kinetic limiting. With every destination valid it sums below 5e-9 on either axis. Restricting publication to one pressure-only tile produces (-0.0616021947,+0.0169290443). Giving open neighbors zero mass produces (+0.0496662826,+0.0565548132). Both exceed the 1e-6 zero-impulse check with visible local variation.
