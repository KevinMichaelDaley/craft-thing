---
id: ctowpe-0zjo
status: in_progress
deps: []
links: []
created: 2026-11-27T15:30:30Z
type: task
priority: 1
assignee: kmd
parent: ctowpe-vtlh
---
# Reduce the downward water-transport spike in the saturated camera workload

Half-native GPU timestamps in a 16-room camera scene isolate the recurring spike to phase 3, transport mode 0. Two downward sweeps took 69.587 and 69.755 ms; horizontal mode 2 took 1.229 and 0.904 ms. Identify expensive work inside the downward sweep and reduce it without altering water, pigments, heat, barriers, clocks, or production cadence. Verify direct GPU equivalence and repeat the same presented-frame benchmark.
