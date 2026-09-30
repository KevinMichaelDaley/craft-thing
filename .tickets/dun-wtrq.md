---
id: dun-wtrq
status: in_progress
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

