---
id: ctlo-f9p0
status: in_progress
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
