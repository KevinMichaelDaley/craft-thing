---
id: ct-alp7
status: closed
deps: []
links: [ct-wuci]
created: 2026-10-13T05:17:54Z
type: bug
priority: 1
tags: [gpu, performance, pixels]
---
# Restore real-time performance for complex pixel-body contacts

User stopped ct-wuci implementation agent after the two-concave-pixel-body visual test ran as a slideshow. Manual headless reproduction of the exact scene measures 142-164ms per steady rigid tick, with 130-155ms in contact generation; warm extraction is about 0.15ms. Parallelize exact contact work, retain collision/material correctness, add the exact scene to measured quarter-native performance gates and verify the real display.

## Acceptance Criteria

Exact scene meets existing real-time GPU budget; old serial path fails a compiling RED performance test. Preserve arbitrary pixel geometry, holes, material/contact keys, saturated rollback and body motion. Full correctness/visual regressions and Vulkan validation pass. No approximation, reduced physics resolution, skipped required contacts, CPU physics fallback or unsolicited agent resumption.


## Notes

**2026-10-13T05:20:44Z**

Exact moving-scene RED: 187.842ms rigid and 193.078ms whole tick. First cooperative narrowphase change distributes each candidate across 64 GPU lanes; same regression drops to 11.876ms rigid/17.045ms total. Rigid budget passes, total 60Hz budget still fails, so continuing optimization without loosening it. Agent remains interrupted.

**2026-10-13T05:54:17Z**

Manual work continues after user interruption. Parallel contact path preserves the scalar path for ordinary bodies; specialized preparation/raster and reset/scalar/pixel contact pipelines avoid unrelated work. The exact scene passes the 60Hz GPU test; the validated 180-tick display run completed in 4.611s. The broader static-structure benchmark still exceeds its 14ms budget (baseline isolated worktree 13.65ms), so per-dispatch raster diagnostics are being collected; not claiming all performance acceptance complete.

**2026-10-13T06:09:26Z**

Final raster optimization builds a conservative, ordered body list once per 16x16 workgroup, including swept bounds and adjacent fluid faces. Actual quarter configuration: static automatic 16-component rigid/structure 10.724ms (14ms gate); exact two-concave-body scene rigid 10.323ms, full GPU tick 15.251ms (60Hz gate). Original exact scene 193.078ms full tick. Full validated headless suite passed before this final raster change; final headless/UI/quarter validation is now being rerun. No geometry or physics-resolution reduction.

**2026-10-13T06:39:34Z**

Final correctness: 283 headless checks, full Wayland UI battery, and quarter-native constituents pass with Vulkan validation after anchoring the formerly isolated stack-test floor (pre-optimization baseline failed identically). Both changed SPIR-V modules validate. Final serial exact rotating scene rigid 11.107ms / full tick 16.350ms; 16-component static rigid/structure 10.976ms. Normal display 180 physics ticks in 3.618s (~49.75 ticks/s), including diagnostic readbacks and pacing. A separate dense zero-body fluid perf gate fails at 20.274ms; baseline comparison under ct-irzt is being collected. Keep GPU budget separate from actual displayed FPS; no claim of whole-application 60 FPS.

**2026-10-13T06:40:25Z**

Detached pushed baseline232d76b dense-fluid result20.240ms versus current20.274ms confirms the separate ct-irzt failure predates this work. Exact-scene and static-structure gates pass; closing this manual slideshow fix. Implementation agent remains interrupted. ct-wuci retains cold-extraction/capacity sweep and warm cached outside-mapped-viewport acceptance.
