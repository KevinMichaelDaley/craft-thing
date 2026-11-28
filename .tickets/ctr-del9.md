---
id: ctr-del9
status: closed
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

**2026-10-17T10:01:45Z**

Validation complete: actual-scene tree-cut app test 2/2 passed under Vulkan core+sync validation, no VUID/SYNC reports. Full make -j4 -i test completed; only the two previously tracked authored_stage_tests failures remain (ctr-ojvy). No implementation change was needed for the reproduced cut with explicit physics steps. Keep issue open awaiting user confirmation of run/pause state.

**2026-11-28T03:59:58Z**

Fresh native component_world_tests run on hidden Xephyr :93 with Vulkan synchronization validation passed 3/3, without validation messages. The actual level_piece01 asset was imported into a temporary world, its trunk was cut with air and after four explicit steps the canopy-sized rigid body had downward velocity. The original user save was untouched. Its historical pause state is unknown. The full suite at this code revision exited zero.
