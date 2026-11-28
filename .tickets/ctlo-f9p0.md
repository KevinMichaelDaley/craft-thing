---
id: ctlo-f9p0
status: closed
deps: []
links: []
created: 2026-10-24T08:45:33Z
type: task
priority: 1
---
# Avoid directory resolution for same-tile carve neighbors

## Acceptance Criteria

GPU work counter fails before optimization and proves fewer than two directory resolutions per remaining pixel. Preserve all pixel coordinates, material, mass, custom RGBA, sparse missing tiles, diagonal connectivity and rotation. Measure actual displayed large cutting frame time.


## Notes

**2026-10-24T08:47:47Z**

RED2/2 fail solely work bound:286132 directory resolutions for36051 remaining pixels. GREEN2/2 actual core/synchronization validation:24422 directory resolutions, same36051 coordinates/materials/customRGBA and mass, bounded110/49 missed repeat checks. Displayed cutting benchmark running.

**2026-11-28T04:25:33Z**

Current GPU comparison measured 12210 directory resolutions for 36051 remaining pixels, with exact shape, custom colors and mass and 110/49 repeat inspections. The full brush suite passed all 13 cases. The subsequent integrated large-cut measurement recorded in ctlo-33fv was 9.791 ms for a 190x190 cut. Its displayed 500-frame workload measured 31.148 ms mean and 36.126 ms worst; this measures the combined changes.
