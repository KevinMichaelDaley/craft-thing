---
id: ctowpe-mbc3
status: in_progress
deps: []
links: []
created: 2026-11-27T17:55:16Z
type: task
priority: 1
assignee: kmd
parent: ctowpe-vtlh
---
# Attribute the slowest saturated camera frames after the X11 presenter change

The two-device camera experiment reduced mean frame time, but p95 remained above 50 ms in both viewport modes. Measure per-frame offscreen owner advancement and resident simulation/presentation on the integrated renderer to select the next optimization.

## Acceptance Criteria

Correlate individual slow frames with owner advances and resident presentation in all three half-native bands on hidden Xephyr. Preserve the production workload, wall clock and physical device, and identify the dominant measured cost.

