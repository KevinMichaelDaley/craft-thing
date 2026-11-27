---
id: cttl-ieib
status: in_progress
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
