---
id: dun-s8rr
status: closed
deps: [dun-wc20, dun-3bl8]
links: []
created: 2026-09-26T07:44:16Z
type: task
priority: 2
assignee: kmd
parent: dun-9det
tags: [gpu, mpm, fluid, coupling]
---
# Exchange momentum between Eulerian water and MPM grains on GPU

Water drag and pressure move grains, while grain motion applies the opposite impulse to the water face field.

## Design

Sample finalized projected water velocity/depth in MPM grid solve; accumulate equal-and-opposite momentum sources in bounded GPU buffers and apply them before the next fluid projection. Keep water volume Eulerian and grains particle-based. Define stability/substep and dry-cell behavior.

## Acceptance Criteria

A flowing stream transports loose sand while still water does not; grain and water momentum changes balance within measured fixed-point tolerance in a closed scene; the coupled scene crosses chunk seams with no normal-frame readback and reports GPU stage timings.


## Notes

**2026-09-27T00:08:44Z**

Current GPU MPM stage treats water-filled cells as excluded from grain movement. Replace this temporary exclusion with projected Eulerian velocity/depth sampling and equal-and-opposite momentum exchange; retain the GPU-only 16x16 indirect active-tile dispatch and fixed-point particle mass.

**2026-09-27T00:29:04Z**

GPU-only two-way MPM/Eulerian drag now permits overlapping water and grains. Per-face reduced-mass impulse is applied with equal opposition after a compute barrier in each MPM substep; dry faces skip exchange. Tests cover wet-grain motion, flowing versus still water, seam crossing, stage timing, and closed x-momentum residual -2.8e-6 (<4 fixed-point units). make test, make test_ui, and Vulkan validation pass.
