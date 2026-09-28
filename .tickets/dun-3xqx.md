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

