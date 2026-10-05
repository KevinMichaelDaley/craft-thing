---
id: ct-vnsh
status: closed
deps: []
links: []
created: 2026-10-05T01:06:56Z
type: task
priority: 0
parent: dun-vd9a
---
# Optimize quarter-native fluid scheduling for 60 Hz

Quarter-native full fluid update baseline on Intel Iris Xe: 25.615 ms/tick (39.04 Hz), fluid 19.025 ms and granular 4.567 ms. Distribute fluid work over two physics ticks while preserving elapsed time and validate convergence/volume/stability.

## Acceptance Criteria

Quarter-native 70-slot dense water/grain benchmark averages below 16.67 ms per physics tick on local Vulkan device; fluid simulation preserves mass, advances at wall-clock speed and remains stable with two-tick scheduling; actual GPU presentation smoke passes.


## Notes

**2026-10-05T01:16:10Z**

RED dense benchmark: 27.127 ms/tick (36.86 Hz). Two-tick elapsed-time cadence passed exact state equivalence at 60/30 FPS and mass conservation, but still missed budget. Fluid-phase timestamps identify pressure and transport costs. With 16 pressure sweeps and cached/empty/single-neighbor transport fast paths, 60-sample dense benchmark passed at 16.322 ms/tick (61.27 Hz), render included 16.923 ms (59.09 Hz). Final correctness/validation and optimized Wayland smoke pending.

**2026-10-05T01:24:02Z**

Complete: quarter-native uses two-tick elapsed-time fluid scheduling and 16 pressure sweeps; transport caches source state, skips empty/unchanged cells, and fast-paths single-cell movement. Dense 60-sample Vulkan budget check passed twice: 16.322 and 16.396 ms/tick (61.27/60.99 Hz). Headless full-grid render included: 16.923/17.061 ms. Wayland interactive smoke: 61.49 presented physics ticks/s and 61.7 adaptive FPS, with 0.973 s simulated versus 0.972 s wall. All 90 headless tests plus 29 quarter-native solver tests pass under Vulkan validation; marker-on/off 1320-tick spray checks both report mean/peak zero airborne pixels. Actual rendered capture and measurements archived in design/native_benchmark.md.
