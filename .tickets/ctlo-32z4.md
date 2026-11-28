---
id: ctlo-32z4
status: closed
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

**2026-11-28T04:09:47Z**

Verified current recording and command-cache suites 5/5 and 10/10. Ordinary runtime records 204 barriers, 10 indirect publications, one host publication and zero invalid dependencies. Extreme origins retain 276 barriers. Cache keys cover overlap changes. Prior synchronized coupled-state and real-time displayed checks are recorded above; design/inline_frame_optimization.md documents enabled overlap with unchanged cadence in the later passing saved-scene profile.
