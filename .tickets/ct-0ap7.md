---
id: ct-0ap7
status: in_progress
deps: [ct-d3oz]
links: [ct-d3oz]
created: 2026-10-31T04:55:31Z
type: feature
priority: 2
assignee: kmd
tags: [gas, thermal, vulkan]
---
# Apply Fourier gas conduction when combustion module is disabled

Current gas-only solver transports total energy by Euler flux and uses practical Rusanov mixing, but the explicit Fourier gas-gas conduction pass is recorded by the thermal/combustion module. General gas-only worlds therefore lack species-capacity-weighted molecular heat diffusion through stationary nitrogen/air.

## Acceptance Criteria

RED gas-only GPU test shows stationary temperature gradient exchanges heat without corresponding species mass exchange, including closed gates and partial cells. GREEN finite-volume Fourier term runs in all gas policies, conserves total energy and mass, uses the shared air conductivity and cell geometry, remains stable for thin cells, passes make test and displayed performance gate.


## Notes

**2026-10-31T05:43:55Z**

Finite-volume gas Fourier flux now runs in fire and gas-only worlds after transport/reaction, with temperature-dependent dry-air conductivity, equal/opposite energy transfer, species-weighted heat capacity and explicit four-face stability. Solid conduction no longer uses the 2x gameplay multiplier. GPU still-air, closed-gate and energy tests pass; full regression rerunning after physical-rate assertions were corrected.

**2026-10-31T07:33:39Z**

2026-10-31: full make test passes, gas display 37 passed/8 capability skips, cut-cell contact4/4, containment1/1. Five displayed bonfire runs passed8/8 dry0/8 wet with gas peaks below1900K. X11 quarter-native 1000-frame benchmark mean16.702 and16.695ms, but worst43.612 and39.760ms at frame108/109, so strict <30ms worst-frame gate fails. Thermal GPU phases remain stable; intermittent broad rigid GPU stage scheduling/presentation spike tracked as ct-d3oz. Keep this ticket in progress until existing strict frame gate passes.

**2026-10-31T08:04:01Z**

Gas heat transport and five displayed ignition runs are verified and pushed; full make -j8 test passes after benchmark/present changes. X11 opt-in immediate present improves mean ~15ms and passed two manual 1000-frame runs, but integrated wrapper still produced one 30.353ms worst outlier from vkQueuePresentKHR wait. Dependency ct-d3oz remains unresolved, so keep gas conduction ticket open until strict displayed gate is reliable.
