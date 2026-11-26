---
id: ctpao-oywm
status: in_progress
deps: []
links: [ctcri-jd9z]
created: 2026-11-26T23:06:20Z
type: bug
priority: 1
assignee: kmd
parent: ct-exwr
tags: [player, vulkan, rigid]
---
# Refresh rigid occupancy after rolling back a staged local player

Admitting a player writes its raster into the world before preparing drive, ray and sprites. A failed admission removes the body record, but its live occupancy can still block the next input before another frame refreshes it.

## Acceptance Criteria

In the SDL/Vulkan join rollback fixture, verify that the failed candidate occupies zero cells immediately after every preparation failure, without stepping the world. Keep cell occupancy for the existing player, and verify that the successful retry appears in the raster.


## Notes

**2026-11-26T23:13:24Z**

The native RED fixture compiled and reported 1,023 occupied cells belonging to failed candidate 2147483650 immediately after the injected drive failure. Its live body record was already gone. The fixture deliberately checks before any world tick or another join can refresh the grid.
