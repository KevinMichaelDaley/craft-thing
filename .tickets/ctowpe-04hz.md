---
id: ctowpe-04hz
status: in_progress
deps: []
links: []
created: 2026-11-27T18:49:17Z
type: task
priority: 2
assignee: kmd
parent: ctowpe-vtlh
---
# Bound page-indexed water dispatches to mapped workspace slots

GPU timestamps on d50c4f60 put 78–83% of sampled replay stage time in water. An independent room uses a nine-slot workspace even when only slot zero is mapped. Page-indexed pressure and mass passes dispatch their full slot capacity, and the shaders immediately return for unmapped slots.

## Acceptance Criteria

For independent workspaces, show actual GPU dispatch dimensions exclude the unmapped suffix. Compare pressure, fluid mass, face velocities, markers and confirmed clocks against full-extent execution with sparse, noncontiguous pages. Exercise admission and retirement after cached commands are recorded. Preserve each pressure sweep and shared-domain behavior. Measure both production camera modes on hidden Xephyr.


## Notes

**2026-11-27T18:54:06Z**

Cache regression passed 9/9. On the RTX A2000 under synchronization validation the real Vulkan calls used depths 1, 6 and 1 as sparse slot five was admitted and retired. Six page-indexed pressure and sixteen mass dispatch recordings per case all had the expected depth. Full pressure fields, mapped cell stock, velocities, sorted marker records and completed fluid ticks matched a resident full-extent reference after every pair of cycles. Camera performance comparison pending.
