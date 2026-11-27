---
id: ctowpe-04hz
status: open
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

