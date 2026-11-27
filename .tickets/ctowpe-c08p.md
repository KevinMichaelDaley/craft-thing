---
id: ctowpe-c08p
status: closed
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

**2026-11-27T17:06:16Z**

At 2048x1216, GPU timestamps for the raw and corrected passes together (6 measured pairs per scenario) gave median times of 7.364/7.774/7.557 ms for the original shader, 3.703/3.945/4.228 ms for the larger per-lane cache, and 0.392/0.928/1.168 ms for eight adjacent columns. An independent repeat of the original shader measured 7.059/7.283/7.427 ms. Each scenario matched all 2,490,368 corrected serial results. Production 64-wide and partial-width 61x1153 tests check raw and corrected output as well as evaluation counts. GREEN passed 12/12 under synchronization validation.

**2026-11-27T17:10:05Z**

Refactor 8f26f761 passes all 16 hydrostatic cases under synchronization validation, including an extra 61x2305 diagnostic beyond the former cache limit. At that height both passes together evaluated exactly 281210 transforms (one per cell per pass), and both output fields matched the serial recurrence. The production 720 loads, both saturated camera viewports and full make test are scheduled from the clean 8f26f761 checkout on hidden :93.

**2026-11-27T17:12:03Z**

Both seam projection cases and all three native/seam water cases passed using the production dispatch after refactoring. Together with the four hydrostatic configurations, 21/21 focused checks passed under synchronization validation. The aggregate regression and presented-frame measurements continue at 8f26f761 under ctowpe-vtlh.
