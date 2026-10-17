---
id: ctr-del9
status: in_progress
deps: []
links: []
created: 2026-10-17T09:56:39Z
type: bug
priority: 1
---
# Imported tree must fall after cutting trunk with air

User cut below imported level_piece01 tree and canopy stayed static. Saved scene has an isolated 296-pixel wood component and no active rigid bodies. Verify pause state and actual scene extraction path; add real asset regression.

## Acceptance Criteria

After a complete air cut and explicit physics steps, the imported canopy becomes a falling rigid body. Preserve original saved world.


## Notes

**2026-10-17T09:58:41Z**

Actual exported scene regression passes with core and synchronization validation: load ground-profile scene, air-cut trunk at world y29 over x8..30, explicitly advance four physics steps, assert canopy-sized extracted body has downward velocity. Saved user world remains untouched. No production extraction defect reproduced; demo initially paused and user pause clarification pending. Test-only diagnostic work; no failing RED fabricated.
