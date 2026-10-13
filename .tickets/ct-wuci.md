---
id: ct-wuci
status: in_progress
deps: []
links: [dun-spbq]
created: 2026-10-12T23:07:46Z
type: task
priority: 2
tags: [gpu, rigid, extraction]
---
# Automatically extract any isolated connected component of rigid-capable pixels

User requires automatic extraction for any isolated connected component of rigid-capable solid pixels, including mixed materials. They explicitly excluded granular materials from this extension. Remove the current 256-cell, 16-cell bounding-side and four-rectangle eligibility restrictions by implementing an exact variable-size pixel/component representation and the corresponding runtime consumers; increasing those constants alone is not sufficient.

Preserve the current 4-connected pixel definition, unless a separately documented connectivity decision changes it. Components may contain holes, concavities, narrow branches and arbitrarily many runs/tiles/pieces. Ordinary rooted terrain remains terrain; fully known isolated components must become moving bodies without a manual spawn or mark command. Unloaded neighbors remain UNKNOWN rather than empty space. Finite memory/output capacity may defer a transaction conservatively, but shape complexity and size must not permanently disqualify a component.

## Acceptance Criteria

- Every fully known isolated component of eligible solid pixels is extracted automatically, regardless of exceeding 256 cells, 16 cells on either bounding-box axis, or four rectangles. Include mixed-material components, long thin shapes, diagonal staircases, branching shapes, hollow rings and components spanning multiple warm chunk seams.
- Geometry is exact: all occupied source pixels and their material assignments are preserved, holes remain empty, and distinct disconnected components remain separate. No convex-hull filling, geometric simplification, lossy rectangle truncation, or accidental connection through diagonal-only touching.
- Variable-size shape storage is consumed correctly by occupancy/rendering, swept bounds/broadphase, contacts, mass/COM/inertia, rotation, buoyancy/drag/displacement, fracture and diagnostic APIs. Extracted components retain a coherent body/assembly identity; decomposition cannot create independently falling unconnected pieces where the source was connected.
- Source terrain removal and body/shape creation commit atomically with material volume, mass, water and linear/angular momentum conserved within documented tolerances. Density and float/sink behavior follow actual pixel material composition; no per-body sink/float toggle.
- Capacity exhaustion keeps one authoritative pending source and retries fairly after capacity returns, without ID consumption, duplication, ghost collision, or material loss. A representable fully resident component is not left permanently as terrain simply because of its geometry or size.
- Cold boundaries freeze extraction until complete support/connectivity is known. World-coordinate identity, variable-size geometry, composition and pending state survive rebasing, eviction/reload and save/reopen; stale page slots never supply false roots.
- TDD includes compiling RED cases proving the old restrictions, GREEN implementations, and regression/visual acceptance with at least two distinct complex concave pixel bodies. Run all headless/UI/quarter-native batteries and Vulkan/SPIR-V validation. Measure cold extraction and unchanged-frame GPU budgets at quarter-native resolution, with complexity/capacity sweeps and bounded work; no ordinary-frame CPU physics solve or readback fallback.


## Notes

**2026-10-13T02:44:59Z**

User confirmed scope: any shape of rigid-capable solid pixels (including mixed materials), not granular sand/dirt/gravel extraction. Resuming the existing implementation agent to implement this ticket with exact variable-size geometry rather than a larger hard-coded shape cap. Epic ct-fx1b remains historically complete; this is the new extraction extension.

**2026-10-13T03:23:04Z**

Initial exact pixel geometry GREEN checkpoint 29ffb4c passes 400-pixel, 40-wide and >4-rectangle concave cases. Version-four checkpoint geometry relocation GREEN f8511ff passes their save/load and diagnostic checks plus all three existing extraction acceptance cases. Full normal regression running. Pixel fracture and fair/scalable extraction, broader consumers/visual and budget validation remain in progress; ticket is not complete.
