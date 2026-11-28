---
id: ctlo-49zd
status: closed
deps: []
links: []
created: 2026-10-24T07:05:45Z
type: task
priority: 1
---
# Load dynamic solver layout capacities once per invocation

## Acceptance Criteria

Each runtime solver invocation reads the current pair/contact layout capacities at most twice total, retaining dynamic growth and exact body/contact results. GPU counters must fail before caching and pass afterward; compare frozen uncached solver and measure real displayed frames.


## Notes

**2026-10-24T07:10:13Z**

RED:83352 layout queries across16384 invocations, exact bodies after pair/contact growth, fails two-query budget. GREEN with actual synchronization validation:32768 queries across16384 invocations, exact bodies and dynamic queue growth preserved;1/1. Intel nonprofiled SOLVE probe drops6889->6085 instructions and389->339 sends with zero spills. Displayed performance pending.

**2026-11-28T04:06:51Z**

Current full GPU suite verified 32768 capacity reads across 16384 invocations versus 83352 in the frozen reference. Body records matched bytewise through pair/contact queue growth. The 500-frame real-time Wayland tree-cut measurement for these three solver address changes is recorded in tools/diagnostics/README.md: 32.414 ms mean, 38.887 ms worst. This measures the combined build.
