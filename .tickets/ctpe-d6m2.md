---
id: ctpe-d6m2
status: open
deps: [ctpe-2lx4, ctpe-847v]
links: []
created: 2026-10-14T03:22:18Z
type: task
priority: 1
parent: ctpe-6m89
tags: [libresprite, assets, animation, props]
---
# Play imported animation and integrate appearance/emissive channels

Implement elapsed-time frame selection/tags/loops/pause/save phase independent of physics; visual-only validates invariant material/role/object channels. Coordinate renderer ABI for albedo/surface/emissive, attached emissions and bounded light-field contribution.

## Acceptance Criteria

Variable cadence yields same frame; pause stable; no per-frame collider/mass replacement. Colors/emission visibly animate and follow body/rest identity through motion/fracture/reload. Actual renderer receives emission; no one-CPU-light-per-texel implementation or duplicate incompatible appearance ABI.


## Notes

**2026-10-14T03:54:31Z**

Offline prerequisite available: compiled64-byte appearance record first32 matches renderer; emissionRGB bytes32..43, visualalpha48, uint32 surface mask60 (normal1/roughness2/relief4). Absent surface bits resolve renderer defaults, not fixed imported placeholders. SourceUUID/object/frame/texel identity immutable. Full tag policy, GPU instance frame selection, pause/save phase and actual GPU field contribution still unimplemented; no16-userpoint truncation or CPUlight-per-texel fallback. Renderer agent confirmed ABI, runtime contract pending its lighting integration.
