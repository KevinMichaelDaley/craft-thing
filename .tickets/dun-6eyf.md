---
id: dun-6eyf
status: in_progress
deps: []
links: []
created: 2026-09-28T02:37:57Z
type: task
priority: 1
assignee: kmd
parent: dun-0t9m
---
# Allow bounded runtime GPU pressure work for offscreen bands

Replace compile-time-only pressure sweep count with a validated per-GPU setting. Keep visible defaults unchanged and allow background bands to use measured lower counts. The GPU still performs the entire projection; CPU config only chooses the iteration budget.

## Acceptance Criteria

Runtime setter changes red-black projection pass count without rebuilding shaders; visible default and half-native 16-sweep quality are preserved; tests verify bounds and meaningful divergence reduction; no per-frame readback.

