---
id: ctowpe-l2hw
status: in_progress
deps: []
links: []
created: 2026-11-27T16:36:42Z
type: task
priority: 1
assignee: kmd
parent: ctowpe-vtlh
---
# Reduce native phase-zero water projection setup

The four phase-zero captures on the saturated native 2048x1216 grid spent 65.849 to 67.341 ms preparing projection fields before the pressure passes. Attribute displacement, field initialization and hydrostatic head work individually. Reduce the measured cost without changing two-tick cadence, water stock, transfer or pressure results.

## Acceptance Criteria

Compare exact GPU fields and native/seam results and demonstrate a reduction in native phase-zero setup with the same 16-owner presented camera workload.


## Notes

**2026-11-27T16:36:52Z**

The same 24-tick native capture showed an additional 19.676–20.047 ms for the phase-zero pressure passes. Downward aperture setup was around 1.2 ms on the native grid. A second timestamp capture separates lateral and upward displacement, the projected-velocity field pass, and the two hydrostatic-head passes.

**2026-11-27T16:52:22Z**

Native instrumented near-camera baseline mean/p95 141.223/206.443 ms. Rigid occupancy and velocity were host-coherent system memory. Reserving mapped local capacity for both reduced phase-zero setup to 14.940–18.200 ms and near mean/p95 to 96.521/125.170 ms. Committed constrained-budget regression and allocation order through 88238843. RED 2 failures out of 3, final 3 pass under synchronization validation. Exact solver/seam, production performance and full regression run scheduled on persistent hidden :93.
