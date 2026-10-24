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
