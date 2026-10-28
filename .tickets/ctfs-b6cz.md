---
id: ctfs-b6cz
status: closed
deps: []
links: [ctfs-5gil]
created: 2026-10-28T19:12:27Z
type: bug
priority: 1
parent: ctlo-6syy
tags: [physics, clock, vulkan]
---
# Advance rigid bodies on the same elapsed clock as thermal and gas simulation

dc_gpu_set_tick_seconds updates tick_time_scale used by fluid/thermal/gas, but rigid_solver.comp uses constant DT=.125 and app/level.c queues one rigid tick per displayed elapsed interval. Faster rendering changes rigid settling relative to ignition and slower rendering slows launcher trajectories relative to combustion. Do not solve by lowering physical clocks or dropping elapsed time; preserve contact robustness and frame budget.

## Acceptance Criteria

Equal physical elapsed time at multiple displayed cadences produces equivalent rigid trajectories/contacts and elapsed thermal/gas clocks; include a real displayed launcher or bonfire case, with no clock cap hiding lost time.


## Notes

**2026-10-28T23:35:05Z**

Rigid scheduler now spends the full elapsed tick budget. Cold launcher regression runs equal elapsed time at 30/60/120 Hz and checks matching arm angles, coal positions, and gas time; all pass. Display fixture uses real rigid release. Frame budget optimization remains tracked by linked ctfs-5gil.
