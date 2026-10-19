---
id: ctr-9e4n
status: closed
deps: []
links: [ct-wuci, ctr-12pa]
created: 2026-10-18T01:27:03Z
type: bug
priority: 2
---
# Extract edited isolated terrain components across all solid materials

Terrain membership is independent of material. Keep loaded components fixed initially. After a post-load change isolates a connected component, extract all its solid pixels into one body, including soil, sand, gravel and debris. Remaining connections to surrounding terrain keep the component fixed. Material-specific support exceptions must not anchor a component to its own included grains.

Preserve pixel material IDs, authored colors and material-derived density through extraction and save/load. Remove included granular particles atomically when publishing the body to avoid duplicating mass. Keep connectivity processing on Vulkan without serial per-column support walks.


## Notes

**2026-10-18T01:43:00Z**

User clarification supersedes material-specific support: connectivity includes all terrain solid pixels. After a post-load edit isolates a component, soil/gravel/etc in it must be part of the same rigid body. Removing the uncommitted downward granular support implementation.

**2026-10-18T02:18:55Z**

Implemented material-independent solid connectivity in local and cross-chunk Vulkan extraction. Terrain dirt/gravel/sand/debris join their component body rather than supplying an external support exception. Added distinct body material mappings, densities, renderer mapping and persistence validation; included granular particles are removed atomically on publication. Initial authored flags and old-save bond migration cover every solid material, preserving edited pixels/colors. Mass reductions now use hundredths and retain legacy material-zero stone defaults. Verification: dense support 7/7 including an 11-pixel mixed-material crown and save/load, sequential detachment 5/5, actual level03 supported-cut then complete-crown-severance scene 1/1. Final make test completed with only the two existing authored_stage_tests failures in ctr-ojvy. Final Wayland scene run passed core and synchronization validation without VUIDs or hazards. Logs build/terrain_component_integrated_full_suite.log and build/terrain_component_integrated_scene.log. Paused prior demo PID no longer exists; reopening the same saved world without resetting it.
