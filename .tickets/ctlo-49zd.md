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

