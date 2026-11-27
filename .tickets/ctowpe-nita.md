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
