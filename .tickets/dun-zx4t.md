---
id: dun-zx4t
status: closed
deps: []
links: []
created: 2026-09-27T01:41:30Z
type: task
priority: 2
assignee: kmd
parent: dun-9det
tags: [gpu, fluid, mpm, ui]
---
# Damp residual water and grain velocity; add interactive dirt floor

Reduce persistent bouncing with tiny velocity damping and expose a soil floor in the coupled interactive scene.

## Design

Apply the same small post-solve velocity loss in GPU water and granular passes without CPU readback; seed a supported dirt surface in the coupled demo.

## Acceptance Criteria

Water and sand horizontal motion decay under isolated conditions; the coupled demo visibly includes a dirt floor and permits wetting it into mud; screenshots and interactive launch run through the Vulkan path.


## Notes

**2026-09-27T01:54:18Z**

Applied 0.999 velocity scaling per granular half-step and at final Eulerian face writeback after marker correction. Unit tests measured wet-face x 0.989010 from initial 1.0 (predictor 0.99, final damping 0.999) and free sand horizontal decay; per-node drag momentum remains balanced after accounting for dissipated momentum. Coupled UI scene now seeds a supported one-cell dirt layer over generated stone and has an open-ended --demo-coupled mode. make test, make test_ui, Vulkan validation, screenshots, and live interactive launch passed.
