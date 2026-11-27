---
id: ctowpe-04hz
status: closed
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

**2026-11-27T18:58:17Z**

The complete six-case Vulkan pressure suite passed, including changes between pressure batches, residual dry pressure and mixed materials. No synchronization-validation errors were reported. Native and half-native measurements are serialized behind the previous runtime revision.

**2026-11-27T19:16:55Z**

Half-native measured on persistent hidden Xephyr :93 at 782cb32c. Near/middle/far mean and p95 ms: 24.397/36.938, 24.113/43.107, 24.362/43.807. All three p95 exceed 33.333 ms. Native run underway.

**2026-11-27T19:20:39Z**

Native near/middle/far at 782cb32c reported 27.792/44.182, 27.067/43.728 and 26.897/44.132 ms mean/p95. Each band had 16 owners and advanced 2.000000104 seconds. Native 720 dry/dense/exact concave p95 were 4.420/11.883/20.552 ms. Both resolution comparisons are complete; the overall 33.333 ms camera gate remains open under ctowpe-vtlh.
