---
id: ctowpe-attc
status: in_progress
deps: []
links: []
created: 2026-11-28T05:21:57Z
type: bug
priority: 1
assignee: kmd
tags: [vulkan, replay, fluid]
---
# Eliminate cross-workgroup bulk-flow velocity races in restored mixed-liquid rooms

The parked-room reopen oracle intermittently publishes a different face velocity on cell 2208 for the second room. Fresh traces show identical timestep endpoints, independent serial submissions, and the mismatch already present immediately after the second GPU tick. Bulk-phase transverse shear reads velocities that adjacent workgroups can overwrite within the same parity.

## Acceptance Criteria

Exact repeated GPU mixed-liquid cells, face velocities and phase state; unchanged constitutive shear, momentum, volume, heat and retained-volume checks; repeat actual parked-room reopen comparison.

