---
id: ct-vnsh
status: in_progress
deps: []
links: []
created: 2026-10-05T01:06:56Z
type: task
priority: 0
parent: dun-vd9a
---
# Optimize quarter-native fluid scheduling for 60 Hz

Quarter-native full fluid update baseline on Intel Iris Xe: 25.615 ms/tick (39.04 Hz), fluid 19.025 ms and granular 4.567 ms. Distribute fluid work over two physics ticks while preserving elapsed time and validate convergence/volume/stability.

## Acceptance Criteria

Quarter-native 70-slot dense water/grain benchmark averages below 16.67 ms per physics tick on local Vulkan device; fluid simulation preserves mass, advances at wall-clock speed and remains stable with two-tick scheduling; actual GPU presentation smoke passes.

