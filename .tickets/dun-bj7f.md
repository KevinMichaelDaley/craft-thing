---
id: dun-bj7f
status: closed
deps: []
links: []
created: 2026-09-27T21:57:19Z
type: feature
priority: 0
assignee: kmd
parent: dun-v2vu
---
# Scale procedural terrain to native pixel density

## Acceptance Criteria

Native one-pixel-per-cell map scales terrain height, basin, cave and surface details by 4 relative to 256x128 view; generation remains seamless across chunk borders and deterministic; default-scale worlds unchanged; native screenshot reviewed.


## Notes

**2026-09-27T22:06:07Z**

Native procedural world samples terrain at 4x scale, including surface, basin, cave and sand details. Scaled seam tests pass; native screenshot inspected. Separate world_chunks_native store avoids mixing existing worlds.
