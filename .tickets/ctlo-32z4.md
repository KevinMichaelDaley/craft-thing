---
id: ctlo-32z4
status: in_progress
deps: []
links: [ctlo-jnt7]
created: 2026-10-24T04:20:46Z
type: task
priority: 1
---
# Overlap independent body and grain apply dispatches

## Acceptance Criteria

Remove redundant body-to-grain apply barriers without fusing kernels or changing timestep, Jacobi iterations, contact rows or correction arithmetic. Preserve ten indirect publications and final host publication, cached-command invalidation, extreme world-coordinate overflow behavior, and exact coupled body/particle results. Validate synchronization and displayed performance with real elapsed simulation time.


## Notes

**2026-10-24T04:37:23Z**

Recording tests 3/3 and command-cache tests 7/7 pass. Coupled solver and dynamic runtime tests pass with core and synchronization validation. Displayed real-time repeated-brush workload 34.377ms mean /44.201ms worst still fails total <30ms. Equal-work fixed-clock diagnostics reference29.729ms versus overlap29.794ms: no demonstrated performance gain; retain conservative default pending evidence.
