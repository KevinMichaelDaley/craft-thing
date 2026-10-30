---
id: ctfs-5gil
status: in_progress
deps: []
links: [ctfs-6t7k, ctfs-b6cz]
created: 2026-10-28T04:18:38Z
type: bug
priority: 0
parent: ctfs-81vw
tags: [fire, performance, vulkan]
---
# Restore displayed bonfire frame budget below30ms before siege expansion

Actual fullscreen Wayland Vulkan benchmark at480x270 physics/render with640x448 halo regressed to101.693ms mean106.760ms worst. First run reached1154 measured frames before120s bound. Functional native tests and full regression are green, but performance is not. Profile and fix inline before coal/siege expansion; do not revisit stable smoke numerics or fake performance through lower physical clock/resolution, flame point caching or headless-only evidence.

## Acceptance Criteria

Real displayed full-frame mean and worst below30ms on this machine over the autonomous finite-fuel bonfire, including all logs catching and sustained moving flames. Preserve current physics/render resolution, normal physical clock and eight rigid logs per pit, wet controls, finite energy/O2 and solid-only subtle2D conduction. Profiling/tools stay in repository; no fixed body counts. Full relevant native integration tests and regression pass, changes pushed.


## Notes

**2026-10-28T18:32:19Z**

Inline continuation: thermal phase specialization improved a short real-clock fullscreen coarse trace from roughly111ms to95.552ms mean/98.797ms worst. Grouped solver reductions95.865/98.084ms and contact boundary cache97.286/100.538ms did not improve this; neither enabled. Combined thermal sample/metadata lookup now passes native water4/4 and combustion feedback6/6. Full performance acceptance remains RED; short profiles are diagnostics, not completion. Investigating exact collision and thermal work reduction; no resolution/clock changes.

**2026-10-28T18:47:51Z**

Full dry-scene replacement stale water-work regression reproduced RED (5 pass/1 fail), then GREEN6/6 including active source disable, water brush wake-up and unrelated partial uploads preserving painted water. Actual fullscreen real-clock32-frame profile improved to81.136ms mean83.613ms worst. Thermal-only full suite passed before this change. Exact filled-rectangle contact RED emitted64 contacts/16 workgroups; GREEN focused12/12 now2 contacts/1 group, large concave unchanged64 contacts/16 groups. Rebuilding scalar-reference comparisons and native bonfire evidence; target still unmet.

**2026-10-28T19:30:23Z**

Experimental exact rectangle path is now behind DC_CONTACT_RECTANGLES=1 and OFF by default after repeated graphical sensitivity; native default also exposed deep-pit ignition/old scorer issues, tracked ctfs-6t7k. Discarded thermal scheduling skip entirely. Keeping thermal specialization/shared lookup and proven complete-dry-replacement tracking. Full suite and native water rerunning before push. Performance remains open; no below30ms claim.

**2026-10-28T19:35:58Z**

Current DEFAULT fullscreen real-clock32-frame diagnostic after gating rectangle prototype OFF:79.871ms mean81.297ms worst at unchanged480x270 with640x448 halo. Complete regression and native water4/4 pass; native corrected bonfire8/8 dry0 wet sustained passes. Target below30ms remains unmet. Next investigate shared-memory storage for small structural equilibrium matrices with exact global fallback and no solver/numerical-policy change.

**2026-10-30T09:35:11Z**

2026-10-30 NVIDIA X11 half-native continuation: persistent device/driver/UUID Vulkan pipeline cache cut repeated GPU window creation from about 74s cold to 1.9-2.6s warm; full bonfire setup reached gas enable in 8.1s warm. Fixture upload is 7.2s (1.4s terrain, 0.05s colors, 5.0s reactive imports). A 32-frame real-clock 960x540 displayed run measured 1133.923ms mean and 1153.675ms worst; first thermal/MPM stage alone about 655ms. Half-native is not within frame acceptance; ticket stays open. Captured image at /tmp/ct_half_bonfire.bmp.

**2026-10-30T09:56:39Z**

Full make test on NVIDIA advanced through catapult 12/12 and most GPU cases, then stopped at pixel_boundary_tests due cross-driver bitwise face roundoff. ct-towz now enforces 1e-5 bounded numerical agreement and passes all four patterns. Subsequent suffix contact_quad_tests remains RED on NVIDIA with one witness feature mismatch and large tied-normal differences; tracked ct-4gtu. Focused gas/practical/cache tests pass, but full regression is not green and frame-budget acceptance remains open.

**2026-10-30T10:11:23Z**

After startup import optimization, X11 quarter-native bonfire_display_tests starts and verifies finite 1800K starter but fails its dt<=0.25f frame guard before autonomous log coverage. Half-native displayed one-frame diagnostic remains 1097.147ms. This is an unresolved real frame performance failure on NVIDIA, not an initialization failure.

**2026-10-30T10:31:59Z**

Final tools/with-vulkan make test after startup changes reached contact_quad_tests and failed its NVIDIA bitwise geometry assertion (ct-4gtu); pixel_boundary_tests passed with strict bounded tolerance. No full-suite green claim. Startup fixture work reduced warm gas-enable time to 5.810s; displayed half-native frame remains about 1097ms.

**2026-10-30T10:42:27Z**

Half-native DC_THERMAL_PHASE_TRACE on RTX A2000 grouped one tick GPU thermal work: sample 82.9ms, prep 77.3ms, transfer 172.2ms, exchange 1.2ms, bonfire-specific 127.3ms, ignition 92.6ms, reaction 120.0ms, finish ~0ms. Cost is spread across repeated full-grid passes; no single buffer relocation can meet 30ms. Device-local samples/fluxes trial passed validation but left stage 650-682ms, so reverted. Trace tool retained in repository.

**2026-10-30T15:50:59Z**

2026-10-30 RTX A2000 X11 same bonfire fixture: rebuilt quarter-native 480x270 real-clock 8-frame mean406.309ms worst409.017ms; temporary fixed 1/60s diagnostic mean303.713ms. Half-native 960x540 fixed-step diagnostic mean912.269ms (real clock previous1133.923ms). Thus elapsed-clock feedback adds ~100/220ms but full-grid GPU work dominates. Quarter trace rigid88ms, thermal192ms, remaining gas/present ~130ms; half thermal ~650ms. The reported 30ms quarter figure is not reproduced by current autonomous bonfire workload. Diagnostic override removed. Next step must reduce grid-wide thermal and rigid work; do not cap simulation clock or report fixed-step as acceptance.

**2026-10-30T16:12:02Z**

2026-10-30 RTX A2000 X11 root cause was host-visible buffer placement. NVIDIA exposes 246MiB mapped VRAM after host system memory in type order; address allocator took system first. RED/GREEN tests now prefer mapped VRAM for large address buffers, route chunk/particle and MPM staging plus component scratch to system, and smoke profile selects existing tested DC_GAS_DEVICE_STATE=1. Rebuilt quarter bonfire 8 frames 17.755ms mean/19.688ms worst, down from406.309ms. Rebuilt half bonfire 8 frames 28.863ms mean/31.409ms worst; 32 frames 32.056ms mean/41.731ms worst at normal clock and 960x540 with1088x704 halo. Device-state gas queue about11-12ms at sustained dt~0.035s; thermal GPU ~13ms in traced slower path. Target below30ms mean+worst over sustained fire remains RED. Cleanup also reports timed-out dirty chunk saves (new ticket).

**2026-10-30T16:27:49Z**

Focused long bonfire_display_tests under smoke profile failed all-eight-log assertion at 60 simulated seconds: 5/8 dry with DC_GAS_DEVICE_STATE=1, 4/8 with mapped state override; wet0/8 both. This is cadence/combustion robustness still RED and is tracked in new child ticket. General gas_display_tests 32 pass/9 fail solely because Large-CFL tests assume unavailable float32 denormal preservation on NVIDIA (ct-xqgz). Full make test stopped earlier at ct-4gtu contact witness divergence.

**2026-10-30T16:28:44Z**

Final quarter-native 32-frame X11 smoke-profile run after all placement changes: 17.007ms mean, 45.122ms worst (frame13), 480x270 active640x448. Half-native 32-frame normal-clock run: 32.056ms mean, 41.731ms worst, 960x540 active1088x704. Gas device-state separate trace run 32.075ms mean35.339ms worst, accepted1 per frame at dt~0.035s and gas wait~11-12ms. Full make test run for ~12min passed through pixel_boundary_tests, then stopped at known NVIDIA contact_quad_tests ct-4gtu. Focused gas_display_tests:32 passed9 failed only on unsupported Large-CFL denorm requirement (ct-xqgz). Focused bonfire_display_tests: all-log assertion failed5/8 dry device-state and4/8 mapped-state at60s (ct-wgqu). Benchmark cleanup dirty-save timeout tracked ct-2k7p. No claim of full acceptance.
