---
id: ct-wuci
status: in_progress
deps: []
links: [dun-spbq, ct-alp7, ct-tu55, ct-hhmy, ct-yd8t]
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

**2026-10-16T20:22:52Z**

Recovery ladder GREEN: full64x64 extraction and immediate motion-cache save integration1/1; production birth now reuses GPU-verified128-lane property helper;3844-pixel mass/COM/inertia exact within existing tolerance; standalone properties3/3 and indirect init2/2;128x64 wet mixed seam with save/load resume1/1 all validation clean. Quarter480x270 rotating exact bodies12.925ms wholeGPU tick/7.735ms rigid passes existing gates. Cold32 ring34.056ms remains above14ms gate; unchanged2.666ms. Cold80/160 sweep not reached due first failure. Full regression/native30FPS still pending.

**2026-10-17T02:51:33Z**

Current checkpoint12f7f66 full make test with Vulkan validation exited0:356 reported passes,0failures,0VUIDs;23SPIR-V modules validate. Actual displayed exact U/comb bodies and legacy concave pair pass, including exact mass and four identical paused frames. Existing480x270 steady GPU gate passes at rigid4.766ms,total10.677ms. Cold ring32/80/160 still FAIL unchanged14ms budget at30.288/26.893/35.647ms; unchanged frames1.766/1.895/2.293ms. Cold solver12.257/11.830/15.962ms and final raster7.716/3.930/7.504ms remain significant. Ticket remains in progress; existing fracture serial work remains outstanding.

**2026-10-17T04:26:06Z**

Parallel MPM activity compaction follow-up ct-lqd8 enables native360 warm frame30FPS mean/p95 acceptance, preserving exact shapes/materials. Current480x270 steady passes rigid4.337ms,total7.063ms. Cold ring32/80/160 remainRED unchanged14ms gate at30.550/32.838/34.907ms; unchanged2.102/2.024/2.294ms. Existing fracture BFS/packing/arena-copy/property parallelization and cold birth properties remain outstanding; no epic completion claim.

**2026-10-17T04:58:42Z**

Cold detailed profiling REDac29ae5: ring32/80/160 cold30.366/32.728/35.249ms, unchanged1.917/2.044/2.401ms. Zero contacts; mass properties cached ready1. Cold contacts8substeps3.76-3.87ms vs warm0.31-0.38; other solver7.70-7.94 vs0.69-0.74; raster6.98-8.64 vs0.69-0.83. Hydro0.60/1.38/4.37ms vs0.059/0.152/0.337. Broad first-frame slowdown needs controlled investigation, not a claim of interior collision geometry cause. Original14ms cold gates remain failing. Log build/diagnostics/cold-solver-detail.log.

**2026-10-17T06:04:52Z**

Audit-only next cold-work candidate, not implemented or measured: the empty-contact solver still records 8 clear/solve/apply iterations. APPLY canonicalizes pose and wraps angles, so blindly skipping iterations would change supported behavior. Any fused empty path must preserve all 8 recurrences exactly and conservatively exclude active structural bonds and overflow; retain normal physics scheduling. The current batched-contact checkpoint is frozen while its full explicit-sync regression runs.

**2026-10-17T06:11:38Z**

Correction to earlier outstanding-work wording: automatic birth mass/COM/inertia are already wired through pixel_mass_properties in extraction phase 23 (commit 2ab7f2e), and current cold fixtures show motion_ready=1. Remaining cold work is the whole rigid-frame budget, not missing birth-property wiring. Existing fracture property/BFS/packing/arena-copy parallelization remains pending.
