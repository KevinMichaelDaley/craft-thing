---
id: ct-tu55
status: in_progress
deps: []
links: [ct-wuci]
created: 2026-10-14T03:55:23Z
type: task
priority: 1
tags: [gpu, vulkan, performance, native720]
---
# Reach 30 FPS with native 1280x720 physics and presentation

User explicitly requires 1280x720 simulation and display on this Intel Iris Xe machine at30FPS. Optimize existing GPU kernels and presentation without hidden upsampling, physics quality loss, geometry simplification, runtime CPU solves or ordinary-frame readback.

## Acceptance Criteria

Measure a real displayed1280x720 window backed by1280x720 physics, whole presented frame <=33.333ms, with GPU stage timings and mean/tail/cold/load sweeps. Include exact concave bodies, density-derived fluids, saturation/complexity and unchanged-work cases. Preserve480x270 quarter-native regression/performance gates and their exact fixtures. GPU runtime work must be parallel/cooperative; serialize benchmark/validation windows only. Run headless/UI/quarter-native/Vulkan/SPIR-V checks and commit/push verified work; do not claim target achieved before displayed measurements pass.


## Notes

**2026-10-14T04:14:46Z**

RED: true1280x720 physics/display with270 cache slots and2 fixed60Hz steps per presented frame: dry mean32.552ms p9536.450; dense water/grains mean71.427 p95101.299. Exact concave birth fails; validated separate seam case above slot128 fails. Existing cadence6/default pressure preserved. GPU lease released to feature worker.

**2026-10-14T04:19:41Z**

GREEN scalability subset: cooperative strided registry search and correctly sized root-prefix dispatch remove128-slot assumptions. Native270-slot exact400 seam passes Vulkan validation; default11 pixel and default/quarter3 acceptance each pass; SPIR-V valid.30FPS remains RED. Native concave presented run stalled in GPU fence wait and was terminated without valid timing; GPU recovery confirmed by correctness suites. Instrumented stage markers added for diagnosis.

**2026-10-14T04:42:05Z**

Device blocker after native stress: restored7509 runtime64x64 hydro probe timed out60sec; independent feature worktree previously4/4 passing8x8 authored staging also timed out45sec with unchanged shader/runtime. No GPU test process remains. No meaningful behavioral RED from these timeouts; global GPU/device recovery needed. Kernel read requires root; sudo -n read-only journal query denied. Native headless isolation previously rigid9.440/fluid17.153/MPM11.806ms; render-only3.075ms. Full regression incomplete.

**2026-10-16T19:48:10Z**

Oct16 recovery: bounded real IrisXe empty/fill/16-invocation compute probe passes with validation; kernel journal has no HANG/reset entries. Safe phase26-only RED ba33ab8 compiles and fails2 cases: dirty0 ANDdirty1 leave indirect triplets27/30 at poisoned grain IDs4369,4369,4369 (about83billion groups each if launched). Test dispatches only one initialization group and never launches poison. Full engine initialization hit15sec process deadline before first submission marker; minimal fixture avoids all-engine pipeline creation. Root cause of historical display crash not established, but stale indirect count defect is directly proven.

**2026-10-16T19:49:15Z**

GREEN bounded phase26 regression: both dirty0/dirty1 now reset tile indirect triplets27 and30 to0,1,1 after arbitrary MPM tile IDs.2/2 pass with validation, each GPU submission one directly dispatched128-lane group and1sec fence bound. Both production Vulkan1.3 and isolated Vulkan1.1 SPIR-V validate. Fix changes only six metadata initialization writes; no geometry, physics, source clearing or quality change. Full engine motion-cache/small-scene validation and timing ladder still pending.

**2026-10-16T20:19:43Z**

CPU audit found a terrain-contact shared predicate race: lane0 could clear group_found before later subgroups read the early-return predicate, leaving divergent barrier participation. Safe source-contract RED fails without running potentially deadlocking shader; GREEN captures private predicate then all lanes rendezvous before any reset. Historical crash causality remains unproven. Full64x64 extraction and immediate cached flag snapshot integration passed validation 1/1.

**2026-10-16T20:29:44Z**

Post-recovery finite15sec native1280x720 physics AND display runs complete without device stall/kernel HANG/reset.2fixed60Hz steps/frame/cadence6 unchanged: dry mean33.706ms p9537.830; dense75.780ms p95105.233; exact concave158.726ms p95261.335/max274.142. Concave final offscreen1tick rigid41.978/fluid40.731/MPM11.882 versus initial isolated rigid~10-12/fluid~18/MPM~14ms: growth during scene evolution needs profiling. Diagnostic cold295.410ms includes explicitly enabled stage isolation, not ordinary cold. Full validated make test running serial GPU lease. Preparing exact parallel hydro-head prefix tests; specialization experiment requested for uniform runtime fluid/projection mode branches after prefix correctness. Branch count alone is not divergence or established crash cause.

**2026-10-16T20:35:52Z**

Additional safe RED0efbbb3 covers hydro phase1 scratch read/reset handoffs. Floor and water-start are captured privately with a workgroup barrier before shared counts reuse; source contracts3/3 and SPIR-V validate, full64x64 bounded cooperative hydro GPU test1/1 passes validation. Prefix RED807f6a2:3 failures against stub,1sec GPU fences; GREEN parallel shader matches literal prior serial CPU recurrence at all46144 cells in each of3 tall721-row/fractional-coverage/UNKNOWN cases (138432 exact outputs), no validation errors. Runtime integration and performance not measured yet. Full pre-prefix regression resumed; GPU windows paused only make between completed child tests, never overlapped submissions.
