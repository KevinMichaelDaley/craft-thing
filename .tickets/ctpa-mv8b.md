---
id: ctpa-mv8b
status: open
deps: []
links: []
created: 2026-11-26T11:18:00Z
type: bug
priority: 2
assignee: kmd
parent: ctlo-r2ap
tags: [fire, smoke, simulation]
---
# Keep the implicit gas clock advancing after a roof carve

The native smoke fixture opens the two source valves, advances 120 ticks, and carves the roof at (328, 49) with radius 4. The immediate edit conserves gas and supplies the opened void from adjacent gas. During the following ticks the implicit pressure solver returns Gas interval is below clock precision. A case with the corrected conservation check reproduces on pinned main b800354f and the atmosphere branch.

## Acceptance Criteria

Trace the rejected transport candidates and local pressure/volume that force the interval below clock precision, then advance the requested interval with finite state and conserved stock. Add a focused implicit solver regression without replacing the pressure policy or reducing the physical carve.


## Notes

**2026-11-26T11:18:16Z**

Pinned baseline diagnostic: /tmp/craft-play-air-smoke-clock-baseline.log. Current branch diagnostic: /tmp/craft-play-air-smoke-green.log. The practical display profile accepts the interval and reaches the independent exhaust-threshold check.
