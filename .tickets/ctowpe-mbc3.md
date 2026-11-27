---
id: ctowpe-mbc3
status: closed
deps: []
links: []
created: 2026-11-27T17:55:16Z
type: task
priority: 1
assignee: kmd
parent: ctowpe-vtlh
---
# Attribute the slowest saturated camera frames after the X11 presenter change

The two-device camera experiment reduced mean frame time, but p95 remained above 50 ms in both viewport modes. Measure per-frame offscreen owner advancement and resident simulation/presentation on the integrated renderer to select the next optimization.

## Acceptance Criteria

Correlate individual slow frames with owner advances and resident presentation in all three half-native bands on hidden Xephyr. Preserve the production workload, wall clock and physical device, and identify the dominant measured cost.


## Notes

**2026-11-27T18:06:16Z**

Profiled 180 half-native production frames at 47e99db4 on hidden :93. Timed dc_offscreen_advance and dc_gpu_present_chunks_steps_progress separately with CLOCK_MONOTONIC around their actual calls. Each band retained 16 owners and advanced 2.000000104 s. All 70 over-budget frames called offscreen advance for 16 owners. Near: 30 slow frames averaged 46.51 ms total, including 35.33 ms offscreen and 10.85 ms resident+present. Middle: 20 slow frames averaged 53.15/42.16/10.72 ms. Far: 20 slow frames averaged 53.89/42.36/10.99 ms. Frames without an owner advance averaged 14.37, 15.80 and 15.58 ms respectively. In the four slowest frames of each band, offscreen work averaged 37.86, 44.15 and 44.56 ms. Current code submits and waits separately for each workspace substep. The archived shared-queue batch is tracked by ctowpe-8rdr.
