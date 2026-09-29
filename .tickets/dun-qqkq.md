---
id: dun-qqkq
status: closed
deps: []
links: []
created: 2026-09-29T02:40:20Z
type: bug
priority: 1
assignee: kmd
parent: dun-v2vu
---
# Blend submerged sand and water within each simulated cell

A cell containing both MPM sand and Eulerian water renders blue at full water mass and yellow when the water mass changes, causing flashing rather than a stable density mixture.

## Acceptance Criteria

Full and partial water cells with a sand particle render a continuous sand-water mixture, distinct from both pure water and dry sand; mixed particles preserve the existing within-cell antialiasing and adjacent cells do not blur together. GPU render tests cover varying water occupancy and the interactive coupled demo.


## Notes

**2026-09-29T02:44:31Z**

GPU render regression reproduced full-water sand cell matching pure water; shader now mixes linear-light sand volume fraction with Eulerian water in the same cell. Dry sand and neighbor cells retain their colors. make test and make test_ui pass; coupled 1-second screenshot inspected.
