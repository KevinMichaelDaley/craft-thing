---
id: ct-tu55
status: in_progress
deps: []
links: [ct-wuci]
created: 2026-10-14T03:55:23Z
type: task
priority: 1
tags: [gpu, vulkan, performance, native720]
---
# Reach 30 FPS with native 1280x720 physics and presentation

User explicitly requires 1280x720 simulation and display on this Intel Iris Xe machine at30FPS. Optimize existing GPU kernels and presentation without hidden upsampling, physics quality loss, geometry simplification, runtime CPU solves or ordinary-frame readback.

## Acceptance Criteria

Measure a real displayed1280x720 window backed by1280x720 physics, whole presented frame <=33.333ms, with GPU stage timings and mean/tail/cold/load sweeps. Include exact concave bodies, density-derived fluids, saturation/complexity and unchanged-work cases. Preserve480x270 quarter-native regression/performance gates and their exact fixtures. GPU runtime work must be parallel/cooperative; serialize benchmark/validation windows only. Run headless/UI/quarter-native/Vulkan/SPIR-V checks and commit/push verified work; do not claim target achieved before displayed measurements pass.


## Notes

**2026-10-14T04:14:46Z**

RED: true1280x720 physics/display with270 cache slots and2 fixed60Hz steps per presented frame: dry mean32.552ms p9536.450; dense water/grains mean71.427 p95101.299. Exact concave birth fails; validated separate seam case above slot128 fails. Existing cadence6/default pressure preserved. GPU lease released to feature worker.
