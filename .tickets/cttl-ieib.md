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
