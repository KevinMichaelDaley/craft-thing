---
id: ctlo-qrwc
status: in_progress
deps: []
links: []
created: 2026-10-24T05:32:10Z
type: task
priority: 1
---
# Compact raster body candidates with parallel bitsets

## Acceptance Criteria

Retain exact ascending body-slot accumulation and all five occupancy/volume/face planes for concave rotated mixed bodies, partial workgroups and more than64 bodies. Replace iterative shared prefix scan with at most four workgroup barriers per64-slot batch, with no total body cap. Validate against frozen scan reference and displayed full-frame performance.


## Notes

**2026-10-24T05:37:17Z**

RED fixture compiles and all five planes match the frozen scan across mixed rotated concave shapes, a123x117 partial grid and128 runtime slots, but both kernels use1920 team barriers against a512 limit. This isolates the candidate-prefix scan without changing geometry or body counts.

**2026-10-24T05:46:57Z**

GREEN normal fixture:1920 ->512 actual team barriers, exactly matching all five planes across128 slots and partial123x117 workgroups. Ordinary displayed real-time500-frame repeated-brush replay:33.916ms mean /43.438ms worst, four paused captures identical. Full-frame gate remains failing. Correction: earlier DC_VK_SYNC_VALIDATE environment variables were not recognized; rerunning with documented VK_LAYER_ENABLES synchronization validation.

**2026-10-24T05:48:36Z**

Actual core+synchronization validation now passes the complete raster fixture using DC_VK_VALIDATE=1 and VK_LAYER_ENABLES=VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT; no VUID or synchronization hazard messages. The previous undocumented DC_VK_SYNC_VALIDATE flag was ineffective. Running corrected synchronization checks for coupled grains, all carving cases and dynamic growth as well.
