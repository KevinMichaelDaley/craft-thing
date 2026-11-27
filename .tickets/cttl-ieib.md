---
id: cttl-ieib
status: closed
deps: []
links: []
created: 2026-11-27T19:07:07Z
type: task
priority: 2
assignee: kmd
parent: ctowpe-vtlh
---
# Bound offscreen water transport sweeps to mapped rows and columns

Each workspace sweep dispatches one group per atlas row or column. In the saturated single-page room this launches 192 groups per direction even when only the first 64 lines intersect a mapped page.

## Acceptance Criteria

Verify all four Vulkan transport dispatch dimensions for partial and sparse workspaces, remapping a high slot between axes without changing slot extent, and empty workspaces. Compare water stock, face velocities, pressure and markers with full-grid execution. Preserve seam and resident dispatch and measure both production camera modes on hidden Xephyr.

## Notes

**2026-11-27T19:11:04Z**

Before the change the RTX A2000 dispatched extra lines for a one-page workspace. After the change all four Vulkan sweeps used 64 columns/64 rows, then 150/64 after sparse admission, 64/143 after moving the same slot to the other axis, and 64/64 after retirement. In all four cases, pressure bytes, mapped cells, face velocity, sorted markers and completed fluid cycles matched the full-grid resident reference. Empty and partial extent recording passed, as did all 10 reusable-command tests. Production comparison pending.

**2026-11-27T19:53:53Z**

Mapped-line dispatch passed 7/7 Vulkan pressure tests and both real seam modes (3/3 each). Production half-native near/middle/far mean/p95 ms: 23.766/38.026, 23.195/40.921, 23.483/41.864. Native: 26.172/39.482, 26.797/44.987, 26.249/42.301. All 16 active owners and clock checks passed. The 720 dry/dense/exact concave p95 were 4.666/13.845/24.423 ms. The parent camera p95 gate remains open.
