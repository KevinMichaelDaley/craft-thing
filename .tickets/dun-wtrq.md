---
id: dun-wtrq
status: closed
deps: []
links: []
created: 2026-09-30T04:05:02Z
type: bug
priority: 0
assignee: kmd
parent: dun-9e5b
tags: [fluid, gpu, quarter-native]
---
# Stabilize quarter-native water source and pool motion

At 480x270 simulation resolution the demo shows upward splashes and persistent pool bounce. Reproduce with quarter-native GPU captures, scale source influx to cell area, and verify the pool settles after injection stops.

## Acceptance Criteria

Quarter-native before/after GPU captures show a bounded source and no persistent upward spray; quantitative source-flow and post-source settling tests pass; exact mass and existing river, seam, marker tests remain passing.


## Notes

**2026-09-30T04:22:28Z**

Quarter-native radius-2 spring injected 8.89 cell-volumes at source edge per tick; radius 1 injects 3.93. At 4 seconds with spring on, high water above basin drops from 54.52 to 21.69 cell-volumes, and mean vertical speed from 0.462 to 0.287. Two seconds after stopping, high water is zero and mean |vy| 0.045. Before/1s/4s/on and after-off captures saved under build/screenshots. make test, make test_ui, and make test_quarter_native pass.

**2026-09-30T04:27:02Z**

User still sees upward jumping water in quarter-native interactive demo after source-radius reduction; prior settling test misses active upward ejection. Reopening for targeted reproduction and physics fix.

**2026-09-30T04:43:53Z**

Reopened after user still saw jumping water. Targeted GPU test shows old active spring delivered 3.93 cell-volumes/tick with max upward face speed 0.89 at 4s; bounded GPU spring delivers 0.99 and max 0.39 at 4s, 0.32 at 8s, with no airborne water away from stream. Explicit spring rate applied to visible and offscreen GPU workspaces. make test, make test_quarter_native, make test_ui pass; 8s screenshot in build/screenshots.
