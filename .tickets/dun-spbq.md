---
id: dun-spbq
status: open
deps: [dun-v75v, dun-9qub, dun-c459]
links: [dun-4ftd, dun-08qq, dun-c459, dun-sbk9]
created: 2026-09-27T02:16:24Z
type: task
priority: 2
assignee: kmd
parent: dun-mi1e
tags: [gpu, rigid, components, gravity, buoyancy]
---
# Extract isolated rigid-material components with automatic gravity and buoyancy

Reuse the GPU root-hooking and size/count buffers from dun-v75v to identify connected rigid-material components, including stone and wood, across resident chunk seams. Automatically convert detached or isolated unsupported components into stable rigid bodies, apply gravity without a manual body-spawn action, and preserve identity while chunks stream.

## Design

Use a material-selectable cell predicate and world-space identity mapping. Resolve merges and splits deterministically on GPU and feed extracted bodies into the existing AABB broadphase and XPBD pipeline without normal-frame copyback.

Distinguish supported or anchored terrain from isolated components; an unloaded neighboring chunk must not be mistaken for empty space. Removing a support or separating a component triggers automatic extraction and gravity. Preserve the component's geometry and material volume; defer extraction safely if bounded body/piece storage is full.

Determine physical density automatically from the component material's physical properties. For mixed-material rigid assemblies, derive mass from the sum of each material's density times its occupied volume, and derive effective density from total mass divided by total volume. Do not require a manually selected sink/float flag or per-spawn density override.

Apply buoyancy from displaced water volume and gravity from the derived mass, with drag and torque based on the submerged shape and fluid motion. Sinking or floating follows the component's density relative to water: sufficiently dense stone sinks; low-density wood floats at the appropriate draft; denser wood may sink. Account for partial immersion and changes to water level or material state. Integrate the stone and wood fluid-coupling work in dun-08qq, dun-sbk9, and dun-c459; preserve rigid and water mass through extraction, streaming, and save/reload.

## Acceptance Criteria

- Connected rigid-material regions spanning resident chunks retain their component identity; disconnected regions remain separate. Labeling, body construction, gravity, and buoyancy stay on GPU without ordinary-frame readback.
- An isolated unsupported component falls automatically. Removing its last support makes it fall without a body-spawn command; supported/anchored terrain remains stable and missing pages do not cause false detachment.
- Density and mass are derived automatically from the component's material composition. Equal-sized stone and low-density wood components respond differently in water: stone sinks and wood floats at a repeatable immersion depth. Denser wood sinks when its material density exceeds water density.
- Partial immersion, changing water level, and flowing water update buoyancy/drag correctly. Mixed-material components use their aggregate material-derived density rather than an arbitrary float/sink classification.
- Chunk seams, eviction, reload, and session saves preserve component identity, geometry, material composition, and physical properties. No rigid-material or water mass is lost, including when extraction storage is saturated.

## Notes

**2026-10-05T04:51:19Z**

User explicitly requested automatic gravity on isolated connected components of rigid materials and material-derived density/buoyancy. Expanded this existing ticket rather than implementing that follow-up in this session, per user clarification. GPU contact generation dun-ci2x remains the current implementation task.
