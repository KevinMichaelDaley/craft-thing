---
id: ct-irzt
status: open
deps: []
links: []
created: 2026-10-05T02:49:02Z
type: task
priority: 2
parent: dun-vd9a
tags: [vulkan, performance, quarter-native]
---
# Investigate quarter-native Iris Xe performance variability

The dense 640x448 quarter-native benchmark previously passed at 60.82 Hz, but current repeated measurements are 46.49 and 46.07 Hz. A detached build of the prior pushed commit 5a98ec5 measured 45.69 Hz under the same conditions, so the slowdown predates the new broadphase. Both versions report rigid stage 0.000 ms (no active bodies); fluid and granular timings increased. Desktop processes were active during the measurement. Keep the 60 Hz assertion intact and investigate runtime load/power variability before changing solver quality.

## Acceptance Criteria

Repeated baseline/current Iris Xe measurements explain the variance and establish a reproducible 60 Hz quarter-native physics budget without reducing simulation correctness; record CPU/GPU load and power/frequency evidence and retain the strict performance assertion.

