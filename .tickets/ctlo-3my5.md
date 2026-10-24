---
id: ctlo-3my5
status: in_progress
deps: []
links: []
created: 2026-10-24T04:41:07Z
type: bug
priority: 1
---
# Look up body motion without rebuilding the host registry

## Acceptance Criteria

Motion updates find active or retained bodies across dynamic capacity without rebuilding host registry, requiring auxiliary ID storage, or changing cached geometry properties. Existing CPU motion and GPU property tests pass.


## Notes

**2026-10-24T04:45:47Z**

New RED test compiled and fails because lookup rewrites body_count (19 to64), followed by the original null-registry fixture crash. Direct bounded record lookup now passes4/4 CPU cases and3/3 GPU property tests with core/synchronization validation; no auxiliary host registry required.
