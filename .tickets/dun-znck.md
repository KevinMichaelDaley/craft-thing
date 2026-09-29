---
id: dun-znck
status: closed
deps: []
links: []
created: 2026-09-29T07:32:05Z
type: bug
priority: 0
assignee: kmd
parent: dun-9e5b
tags: [fluid, gpu, pressure]
---
# Stabilize a 32-cell-high GPU river after a surface displacement

Large rivers remain perpetually bouncy despite per-face viscosity and wall friction. Add a two-chunk river 32 cells deep, disturb its surface, capture rendered before/1s/4s frames, and measure late vertical speed, surface overshoot, and exact mass. Fix the large-domain pressure/hydrostatic cause on GPU.

## Acceptance Criteria

The 32-cell-deep river loses its oscillation over four seconds without large high splashes, preserves exact water mass, crosses the resident chunk seam, and retains interactive half-native tick performance. No per-frame CPU readback.


## Notes

**2026-09-29T08:49:15Z**

Two-chunk 32-cell river conserves 4012 cell-volumes exactly and has no water above y=22 at 1s/4s. Mean |vy| falls from baseline 0.0518 to 0.0218 cells/tick at 4s. Full make test, test_ui, and test_half_native pass; half-native smoke: 63.07 physics ticks/s, 60.2 presented frames/s, fluid GPU 2.4 ms/tick. Water-only deep chunks use 1.9 SOR; particle chunks retain 1.5. Pressure coupling across workspaces remains tracked by dun-r1a0.
