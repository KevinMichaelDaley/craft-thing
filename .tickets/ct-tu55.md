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
# Reach 30 FPS with native 640x360 physics and presentation

Current user request (October16) supersedes the earlier720p target: require640x360 simulation AND display on this Intel Iris Xe machine at30FPS. Preserve measured720p results below as historical reference. Optimize existing GPU kernels and presentation without hidden upsampling, physics quality loss, geometry simplification, runtime CPU solves or ordinary-frame readback.

## Acceptance Criteria

Measure a real displayed640x360 window backed by640x360 physics, whole presented frame <=33.333ms, with GPU stage timings and mean/p95/cold/load sweeps and actual displayFPS. Preserve2 fixed60Hz physics steps per presented frame, existing cadence/pressure/quality and exact1176/1872-pixel concave shapes/materials; reposition fixtures transparently for the smaller world. Include density-derived fluids, saturation/complexity and unchanged-work cases. Preserve480x270 quarter-native regression/performance gates and their exact fixtures. GPU runtime work must be parallel/cooperative; serialize benchmark/validation windows only. Run headless/UI/quarter-native/Vulkan/SPIR-V checks and commit/push verified work; do not claim target achieved before displayed measurements pass. No180p physics upscale substitution. Stop treating720p30FPS as the current target.


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

**2026-10-16T20:49:17Z**

Integrated prefix correctness GREEN: production shader is now tested directly by3-case minimal fixture;138432 exact head outputs. Normal runtime head readback matches closed pool, projected divergence0, displacement/conservation, UNKNOWN and six-phase equivalence each1/1 validation pass. Initial wiring RED was Makefile recipe accidentally compiling projection.comp under hydro filename; separate source rules fix it. Temporary serial reference compare removed from C/build targets. Quarter whole12.369ms/rigid8.372ms passes, cold32 ring32.826ms remainsRED. True1280x720 concave mean157.692ms p95262.478: no material gain from prefix alone. Final rigid profile extraction0.042/prepare0.077/solver25.785/raster+contacts+structure10.182ms,2 active pixel bodies both cached,19 pairs/0 contacts/no overflow. Full regression resumed; remove obsolete projection mode4 in REFACTOR after old suite binaries finish, then rerun final suite. Next measured fluid specialization experiment and wider hydro force clipping work distribution.

**2026-10-16T21:55:15Z**

User explicitly changed current target to native640x360 physics AND display30FPS. Benchmark now parameterizes dimensions;360p maps60 chunks with the same30-slot warm reserve (90slots),2fixed60Hz steps/frame,cadence6 and default pressure preserved. Exact two concave shapes retain1176/1872 pixels/materials; positions are x=width/2-120,y=height/6 and relative18/95 offset, matching old720 fixture at1280x720.720 measurements remain reference only. First360 baseline compiling; no new720 stress runs.

**2026-10-16T21:56:46Z**

360p RED baseline, actual640x360 physics/display scale1,90slots,2fixed60Hz steps/frame,cadence6/default pressure unchanged: dry mean9.402ms p9510.775 max11.145 cold24.695 (106.364FPS); dense21.096 p9529.384 max32.459 cold37.542 (47.403FPS), steady gates pass. Exact1176/1872-pixel concave scene mean129.772 p95181.184 max183.255 cold119.440 (7.706FPS),30FPS gate fails. Final1tick rigid31.925/fluid35.526/MPM3.508ms. No diagnostic extra steps in these cold timings. Focus optimization on supported360p concave case;720reference only.

**2026-10-16T22:09:34Z**

Native 640x360 motion-grown scene GPU timing: hydro19.661ms, eight contact passes2.384ms, remaining solver1.154ms, raster/structure5.978ms. Interior pixels are rejected before contact polygons; allocated tile traversal remains. Fluid same-blob generic/specialized whole-frame129.484/128.337ms, negligible benefit. RED35e50a8 requires wet62x62 force integration distributed into16workgroups, max4cells/lane. Implementing deterministic GPU prefix reservation and partial reductions; no quality/resolution changes.

**2026-10-16T22:13:12Z**

Parallel wet force checkpoint99745e1: exact62x62full-immersion test passes16groups, <=4cells/lane, analytic vertical impulse and zero torque under validation. Motion-grown native360 hydro19.661 ->0.763ms, total solver23.199 ->4.339ms; contact2.384 ->2.413ms. Presented mean128.272 ->76.534ms, p95180.439 ->122.451ms,13.066FPS (still fails30FPS). Full validated suite running; no completion claim. Counters are enabled only for diagnostic profile ticks; normal frames unchanged.

**2026-10-16T22:20:55Z**

Contacts-only source/measurement diagnosis: boundaries are filtered before polygon work, but legacy-A/pixel-B formerly ran the whole B query in1lane. REDb64fcd0 records counters-free64x64solid10.235ms versus0.680ms reversed, identical2contacts. Cooperative B-query GREEN candidate0.556ms versus0.675ms, identical counts; validation passes and SPIR-V validates.96comb mixed geometry retains553contacts. Full correctness regression continuing; no FPS acceptance claim.

**2026-10-16T22:34:45Z**

Runtime range-cache integration RED: rotated14x12body plus fast bidirectional128x64fluid fixture. Original transport fingerprint97a0ea02c0e0a633, mass339325648;8192cache mismatches as expected because cache is not wired. Independent cache/query tests already3/3exact+validation. Preserve this fluid output fingerprint when adopting cached queries; no ordinary-frame readback/CPU solve.

**2026-10-16T22:44:37Z**

Integrated exact cache preserves original coupled fingerprint97a0ea02c0e0a633/mass339325648 and body-free993637574fbe0c8f/mass230613072. Five fluid correctness cases pass validation. Native360 clean74.985ms mean/117.995p95 (13.336FPS); diagnostic74.544mean vs prior81.556, not30FPS. Transport20.873/6.473/46.613/51.070ms/cycle; cache builds0.48ms/cycle. Exact ordered line traversal remains dominant; considering cooperative per-line recipient prefix allocation, not claiming fully parallel recurrence. Iris Xe reports subgroup32, arithmetic/basic supported(operations0x6ff).

**2026-10-16T22:50:05Z**

Cooperative transport RED43cee73 -> GREEN: six minimal GPU fixtures match the literal original ordered source/destination recurrence with zero cell-water mismatches under Vulkan validation, including four directions, fractional apertures, odd137x83bounds, velocity trapezoid and UNKNOWN pages. One complete subgroup perline uses parallel blocker minima and integer exclusive recipient-prefix allocation;2048-cell shared water cache, GPU global cache-miss path forlongerlines. Not runtime-wired yet; no performance claim. No workgroup barrier behind returned subgroups.

**2026-10-16T23:07:21Z**

Immutable per-line field cache keeps all 18 recurrence tests and exact coupled fingerprint passing. Cache extent is specialized to actual line length (maximum 2048 cached cells; larger lines retain exact cooperative GPU global accesses). Native 640x360: 48.429 ms mean, 57.373 ms p95, 20.649 FPS; fluid 10.090 ms/tick versus 15.321 before field caching. Updated transport pass costs 6.714/2.564/9.009/10.412 ms per six-tick cycle. Alternate separate shared arrays gave no improvement (49.914 ms mean); retaining simpler shared cell records. Thirty FPS remains unmet; raster/structure ~5.7 ms/tick and displacement ~15.3 ms/cycle remain targets.
