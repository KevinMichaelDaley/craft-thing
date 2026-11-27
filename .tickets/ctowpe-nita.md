---
id: ctowpe-nita
status: in_progress
deps: []
links: []
created: 2026-11-27T17:13:23Z
type: task
priority: 1
assignee: kmd
parent: ctowpe-vtlh
---
# Skip water-displacement setup for lines without a rigid body

The native phase-zero trace spends 6.59–9.76 ms on upward displacement alone. Each row and column initializes water and bulk metadata and writes the cached water back even when its rigid occupancy plane contains no body.

## Acceptance Criteria

Skip line setup when there is no rigid occupancy on the line. Preserve exact water distribution, including unknown pages and lines exceeding shared cache capacity. Verify GPU work for both sweeps and compare native-size timing.


## Notes

**2026-11-27T17:13:55Z**

RED exact masses match, but the first upward test prepared all 137 columns when only 64 had body occupancy.

**2026-11-27T17:19:37Z**

GREEN passes all 18 focused displacement cases under synchronization validation. On a 2048x1216 grid with a 64x64 occupied area, baseline GPU timestamps for upward/lateral were 6.927/2.235 ms with all pages and 7.077/2.274 with a missing page. Skipping unoccupied lines measured 0.425/0.313 and 0.403/0.327 ms with identical water in every resident cell. Repeated timings, seam checks and production camera gates are in progress. Refactor 1abdfaa8 has an independent full regression job on persistent hidden :93, serialized with the 8f26f761 run.

**2026-11-27T17:21:46Z**

Independent repeat: old upward/lateral 7.022/3.389 ms with all pages, 8.464/2.138 with a missing page. New 0.436/0.315 ms and 0.413/0.302 ms. Every resident water value matched its serial reference in both repetitions of all four scenarios. Normal, wide, tall and profiled displacement passed again after refactoring.
