---
id: ctowpe-bvwf
status: in_progress
deps: []
links: []
created: 2026-11-27T20:39:47Z
type: task
priority: 1
assignee: kmd
parent: ctowpe-vtlh
---
# Reduce resident simulation time on saturated native camera frames

Resident GPU ticks take 13.69 to 17.87 ms in the slowest native frames. Attribute those ticks to physical phases and optimize the dominant cost without changing simulated state or pacing.

## Acceptance Criteria

Measure rigid, particle, bulk and six scheduled water phases on the native workload. Demonstrate preserved solver results and repeat uninstrumented native, half-native and 720p benchmarks on Xephyr.

