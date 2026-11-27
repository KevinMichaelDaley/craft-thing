---
id: ctowpe-c08p
status: in_progress
deps: []
links: []
created: 2026-11-27T17:00:59Z
type: task
priority: 1
assignee: kmd
parent: ctowpe-vtlh
---
# Avoid duplicate hydrostatic transforms on native-height grids

At 2048x1216 the two hydrostatic passes recalculate every cell transform in the output pass, including neighbor searches in covered cells. Preserve the serial recurrence while reducing the duplicate GPU work.

## Acceptance Criteria

Raw and corrected passes evaluate each cell transform once at native height. Verify exact heads for partial cover and missing pages, compare GPU timestamps at native dimensions, and retain existing behavior on taller grids.


## Notes

**2026-11-27T17:03:08Z**

RED reproduced on Xephyr :93. Each raw and corrected pair evaluated 281332 transforms for 70333 cells, twice the 140666 budget. Both fields matched their serial references for all three scenarios.
