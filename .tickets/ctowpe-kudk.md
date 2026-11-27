---
id: ctowpe-kudk
status: in_progress
deps: []
links: []
created: 2026-11-27T15:57:04Z
type: task
priority: 1
assignee: kmd
parent: ctowpe-vtlh
---
# Keep native hydraulic aperture scratch in device-local memory

The half-native downward setup took 15.6–15.8 ms on shared host staging and 0.128 ms when both scratch buffers landed in local memory. Allocate dedicated native transport storage sized to the grid and both aperture tree orientations while preserving host-readable component labels, seam owner layout, and exact transport.

## Acceptance Criteria

Native transport does not modify terrain component labels. GPU transport/reference and coupled water tests pass. Repeat the same saturated camera diagnostic and compare downward setup under the same workload.
