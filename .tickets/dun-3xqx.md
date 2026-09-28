---
id: dun-3xqx
status: open
deps: [dun-bd93]
links: []
created: 2026-09-28T02:43:17Z
type: task
priority: 1
assignee: kmd
parent: dun-0t9m
---
# Schedule offscreen GPU steps by distance on the same world clock

Run nearby world chunks at lower cadence with larger stable timesteps and fewer pressure sweeps, farther chunks still less often, then sleep beyond four screen widths. Carry exact elapsed time, cap steps by velocity/CFL and free-surface tests, and catch up before visible promotion.

## Acceptance Criteria

One/two-screen pan tests show offscreen evolution; four-screen band sleeps only beyond threshold; same elapsed time gives similar progress across render frame rates; GPU timings prove bounded extra work.


## Notes

**2026-09-28T03:05:22Z**

Elapsed-time GPU MPM timestep scaling is implemented; dynamic MPM substeps preserve one-cell gather CFL at long ticks. Water and sand pan regressions pass. Remaining: cross-band conservation, equal-world-time camera-cadence benchmarks, native work budget.

**2026-09-28T03:13:53Z**

Half/native smoke benchmark confirms offscreen GPU work budget is critical: foreground alone ~16.7/9.5 ticks per second. The 4/12/24 cadence uses fewer pressure sweeps but still dispatches full context; profile saturated ring before closure.
