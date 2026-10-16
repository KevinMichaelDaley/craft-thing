---
id: ct-wuci
status: in_progress
deps: []
links: [dun-spbq, ct-alp7, ct-tu55]
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

**2026-10-13T06:12:08Z**

Agent remains stopped at user request. Parent manually repaired the slideshow under ct-alp7; exact quarter-native scene now meets GPU 60Hz budget without shape or resolution reduction, and final regressions are running. Implemented checkpoints include sparse exact tiles, V5 geometry persistence/V4 upgrade, material-preserving fracture, fair cursor retry, regional contacts, occupied residency and invariant fracture poses. Ticket remains in progress: warm cached components outside mapped viewport and final cold-extraction/capacity/complexity sweep acceptance are outstanding.

**2026-10-13T06:35:37Z**

Final quarter battery exposed an obsolete stationary-floor fixture: its 50x1 painted platform is now eligible for automatic extraction. Parent reproduced identical stack failure on pre-optimization abde3d6 (same y=20.156403, angle=-1.524839), so it is not a ct-alp7 runtime regression. Fixture now connects the platform to ordinary terrain, retaining all original settling/motion/density/persistence assertions.

**2026-10-13T19:35:00Z**

Resumed with explicit user requirement: runtime extraction and component-sized/count-sized/arena-sized work must use GPU-parallel passes, cooperative workgroups, reductions/scans and deterministic transactional allocation; no single-lane whole-component packing/mass or whole-arena compaction. GPU benchmark scheduling remains serialized for valid measurements.

**2026-10-14T03:31:58Z**

Parallel cache-atlas birth GREEN passes11 component and8 pixel-world cases with Vulkan validation and SPIR-V validation: cooperative shared-memory local connectivity, parallel seam union/find, hierarchical root/directory scans, deterministic collision-safe ID/body/arena reservation, parallel source packing/clearing, cooperative geometry copy and integer/compensated property reduction. Parent contact/raster optimizations preserved. New cooperative pixel hydrodynamics removes whole-bounding-area single-lane scans and reconstructs hydrostatic columns via parallel reductions; exact two-concave-body quarter GPU rigid7.652ms/whole12.796ms. Cold whole first-active tick remainsRED28.527ms (extraction~9.7ms; first solver12.36ms);14ms assertion unchanged. Pixel fracture still uses serial BFS/packing and remains outstanding; ticket stays in_progress.

**2026-10-14T03:55:23Z**

Scope split at user request: optimizer retains performance/parallelization of EXISTING fracture BFS, packing, property reductions and arena copying, preserving current exact geometry/physics and transaction semantics. Separate feature worker on feat/structural-fracture-features from6e723cd owns only demonstrably missing NEW behavior and atomic multi-role authoring/appearance integration. Native1280x720 physics AND display30FPS is a separate optimizer target;480x270 gates remain unchanged.

**2026-10-14T04:42:05Z**

7509bcc verified extraction/hydro/scalability milestone pushed to origin/main. Full make test retry could not complete during device-wide GPU stalls; independent unchanged feature8x8 workload also stalls. Restored verified runtime and removed speculative barrier changes. Exact acceptance fixture default3 andquarter3 passes; all remaining performance/fracture/visual acceptance stays open. Shared128-lane property helper drafted unwired, standalone SPIR-V compiles/validates but GPU verification pending recovery.

**2026-10-14T04:56:45Z**

CPU-only cache metadata RED committed2a495a9: motion edits invalidated immutable pixel mass/COM/inertia and forced whole-shape scalar recomputation. Minimal setter/cache refresh implementation now passes3 host-contract cases with no Vulkan submission; pending GPU inverse-flag, dormant-source and snapshot validation. No new snapshot ABI state added. Renderer independent unchanged96-texel reference probe localizes device stall after successful context/pipeline creation at texture submission; timed out25sec. No extraction/hydro in that submission; all GPU validation blocked pending recovery.

**2026-10-16T20:07:47Z**

Oct16 safe GPU acceptance: draft128-lane pixel_properties helper now passes mixed concave L5 closed-form mass/COM/inertia/density for wood+stone+dense wood and mortar.3/3 bounded tests pass with validation; actual production rigid PREPARE shader refreshes all4 flag combinations plus dormant converted-source flags while intrinsic property bits and42 geometry words remain identical. Each submission one workgroup/1sec fence. No kernel HANG/reset entries. Tests use minimal pipeline fixture; runtime helper wiring and end-to-end snapshot/full-suite checks remain pending. CPU-only engine init trace completed after cache warmup with GPU submissions forbidden; first structural phases4/5 compile30.456/26.232seconds, later cached~0.25sec.
