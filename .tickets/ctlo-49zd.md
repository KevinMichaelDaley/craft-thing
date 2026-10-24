---
id: ctlo-49zd
status: in_progress
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
