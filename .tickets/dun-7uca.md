---
id: dun-7uca
status: closed
deps: []
links: []
created: 2026-09-27T21:50:27Z
type: feature
priority: 0
assignee: kmd
parent: dun-9e5b
---
# Cadence native fluid update every six physics ticks

## Acceptance Criteria

Configurable fluid update interval defaults to one; native interactive mode runs fluid and markers once per six rigid/granular ticks, with no GPU copyback, and tests verify exact cadence and unaffected rigid/granular progression.


## Notes

**2026-09-27T22:06:07Z**

Eulerian fluid update split into six GPU phases: projection preparation and 40 pressure passes across ticks 1-4, final 2 passes + transport + markers on tick 5, correction on tick 6. Rigid and MPM run each tick. Full-grid uncoupled equivalence and cadence tests pass. Native frame range improved from 37.3-509.7 ms to about 76-138 ms.
