---
id: dun-4w8r
status: in_progress
deps: []
links: [dun-z5fb]
created: 2026-09-29T03:33:02Z
type: task
priority: 1
assignee: kmd
---
# Drive cold chunk activation from GPU wet frontier in both axes

## Design

Current dun-z5fb fix prefetches the last painted water row horizontally into four lazily created workspaces. Replace that heuristic with GPU activity metadata and a budgeted sparse workspace scheduler; do not copy full chunks per frame.

## Acceptance Criteria

Fluid crossing any side of a resident workspace activates the next cold chunk at bounded readback cadence or via GPU residency metadata; support multiple active water elevations; continue toward equilibrium across the configured offscreen physics band without the paint-history row or four-workspace limit.

