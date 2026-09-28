---
id: dun-6eyf
status: closed
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


## Notes

**2026-09-28T02:42:56Z**

Runtime per-context pressure sweeps now support 4–32 passes; changes are rejected mid-solve. The 30-test fluid suite and full headless suite pass. Offscreen scheduler will choose lower budgets only after divergence measurements.
