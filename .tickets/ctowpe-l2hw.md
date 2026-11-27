---
id: ctowpe-l2hw
status: closed
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

Native instrumented near-camera baseline mean/p95 141.223/206.443 ms. Rigid occupancy and velocity were host-coherent system memory. Across four comparable captures, reserving mapped local capacity for both reduced phase-zero setup from 68.064–71.493 ms to 14.940–18.713 ms. Near-camera mean/p95 fell to 96.521/125.170 ms. Committed constrained-budget regression and allocation order through 88238843. RED 2 failures out of 3, final 3 pass under synchronization validation. Exact solver/seam, production performance and full regression run scheduled on persistent hidden :93.

**2026-11-27T17:09:41Z**

Verified on production revision 88238843. The 24 focused checks for allocation, exact heads, displacement, pressure, seam pressure, and seam water with marker correction enabled and disabled passed with synchronization validation. At 1920x1080 the three saturated bands measured mean/p95 95.165/121.557, 93.551/119.526 and 87.842/114.234 ms, versus 142.140/203.249, 140.944/183.708 and 138.454/206.278 before residency priority. All bands had 16 active owners and advanced the world clock 2.000000104 seconds. The phase-zero reduction is verified. The remaining 30 FPS requirement is tracked by ctowpe-vtlh.
