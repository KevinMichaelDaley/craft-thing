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

