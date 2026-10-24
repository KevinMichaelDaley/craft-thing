---
id: ct-tu55
status: in_progress
deps: [ct-fmj6, ct-lqd8, ct-79mx]
links: [ct-wuci, ct-k6ym, ct-sqg7, ct-79mx, ct-irzt]
created: 2026-10-14T03:55:23Z
type: task
priority: 1
tags: [gpu, vulkan, performance, native360]
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

**2026-10-16T23:44:19Z**

Raster runtime RED5785631 -> GREEN: GPU-only classifier now routes pixel-only and mixed/legacy raster pipelines through fully reset indirect triplets. Two different rotated mixed-material concave bodies (324/574pixels) on an odd123x117grid produce byte-identical five occupancy/volume/face planes for generic and specialized paths across four paused frames. Validation clean. Skipped structural refresh still executes the bounded classifier to zero both dispatches. No CPU body-type readback and no geometry/resolution change.

**2026-10-17T00:02:59Z**

Integrated cooperative displacement runtime passes Vulkan validation: exact prior fingerprint 97a0ea02c0e0a633 and mass339325648, aperture mismatches0. Column/lateral displacement now256/128 workgroups instead of4/2 in128x64 two-update fixture. Native360 measurement follows; no30FPS claim.

**2026-10-17T00:05:02Z**

Full validation RED: pixel_properties_gpu_tests compiles, first two reductions pass, prepare fixture emits VUID07988/08114 because new shared rigid classifier binding27 is absent from minimal layout. Full suite stopped at invalid fixture dispatch. Correct fixture resource declaration before restarting; not a production driver hang.

**2026-10-17T00:07:36Z**

Native 640x360 physics and display displacement checkpoint: clean 30-sample concave mean 44.114 ms, p95 56.462 ms, cold 93.263 ms, 22.669 FPS; target remains RED. Diagnostic run 44.150 ms confirms displacement column 1.073 + lateral 3.441 ms per six-tick fluid cycle (~15 ms before), transport 9.763/4.329/8.098/12.565 ms; final hydro 0.758 ms, contacts 3.783 ms, other solver 1.270 ms. MPM CPU compiler experiment: generic SIMD8, 8551 instructions, 440 non-SSA registers, 7.55 s JIT; grid specialization SIMD16, 719 instructions, 61 registers, 0.452 s JIT; zero spills both. GPU A/B pending full regression; not a claimed speedup.

**2026-10-17T00:36:05Z**

Cooperative runtime full validation complete: make test exit0, 341 reported passes, zero failures and zero Vulkan validation errors. Log build/diagnostics/cooperative-fluid-full-test-retry.log. No runtime cleanup change was needed after review; actual displayed concave/UI and quarter performance checks remain queued. Thirty FPS and cold extraction gates remain open.

**2026-10-17T00:47:34Z**

Visible-body native baseline corrected: concave fixture now explicitly selects material/structural rendering. 640x360 physics/display: mean 36.921 ms, p95 45.220 ms, cold 104.098 ms, 27.085 presented FPS; still RED. Earlier native timings used overlay NONE, which omitted body material drawing. MPM same-blob generic/specialized/original controls were 35.961/35.896/36.240 ms with indistinguishable GPU MPM time; no runtime split adopted. Both actual concave visual tests pass validation, contacts/conservation and stable paused images. Typed-draw RED: missing pipeline handles and hidden enabled legacy classification fail as expected; eight-selector and image tests compile.

**2026-10-17T00:55:51Z**

Typed drawing passes eight GPU selector cases plus byte-identical paused images across all five overlays, cropped viewport and hidden enabled legacy fallback. Physical five-plane occupancy/volume/face output remains identical. Visible native640x360 mean35.789ms,p9544.059ms,cold103.990ms,27.941FPS versus36.921ms generic baseline. No30FPS claim. Both draw variants use GPU/configuration selection; no CPU geometry scan. Classifier indirect words now have one final writer each, removing cross-lane reset/overwrite ordering dependence. Full headless341-pass checkpoint predates this draw change; final broad rerun pending.

**2026-10-17T02:18:10Z**

Typed contact pipelines and compatibility guards pass targeted Vulkan selector and exact 44-contact body/terrain/grain equivalence tests. Visible native 640x360 simulation and display: mean 33.768 ms, p95 41.021 ms, cold 102.650 ms, 29.614 FPS; target remains unmet. Full suite must be rerun after this checkpoint.

**2026-10-17T02:19:58Z**

Reuse already-proven transport recipient residency for exact cache/global reads. All18 literal cases, coupled hash97a0ea02c0e0a633 mass339325648, and full-recipient work budget pass. Two visible native640x360 runs:33.673/33.720ms mean;41.042/41.688ms p95;102.557/102.690ms cold;29.698/29.656FPS. Fluid6.959/6.955ms per tick. Small measured improvement;30FPS remains RED.

**2026-10-17T03:03:18Z**

Exact request reuse, fully-open aperture proof,32-lane transport team, and mutable free-capacity summary pass18literal transport cases, coupledhash97a0ea02c0e0a633/mass339325648, SPIR-V validation and CPU shared-memory sizing checks. New committed RED long-range full-recipient budget now passes:21.650ms baseline to0.950ms; no-water/no-capacity exits prove no physical change. Visible native640x360 repeat32.462/32.780ms mean,38.616/39.044ms p95,97.989/102.148ms cold,30.805/30.506FPS. Mean passes30FPS but p95 gate remains RED; not overall target completion. Full regression must rerun after this transport checkpoint.

**2026-10-17T03:08:27Z**

Immutable source bitmap skips only initially ineligible or zero-allowance sources; prior ordered transfers cannot populate unvisited sources. Bounded word scans preserve exact priority and uncached fallback.30literal oracle cases (including one-unit quantization in all directions) and coupledhash97a0ea02c0e0a633 pass withvalidation. RED subunit work ratio15.875 now1.009, waterunchanged. Native concave mean30.752ms,p9534.804ms,cold102.398ms,32.518FPS: mean passes, p95 stillRED33.333. Dry9.714ms/p9511.882/102.949FPS anddense21.794ms/p9531.582/45.883FPS pass; fullregression pending afterthischeckpoint.

**2026-10-17T03:12:21Z**

Contact recorder removes intermediate publication barriers between atomic candidate passes, retaining reset-to-indirect and final-to-solver/host dependencies. CPU recording tests2/2 pass; exact44-contact/occupancy/image comparison and9contact cases pass with synchronization validation explicitly enabled (noSYNC/VUID diagnostics). Native run31.120ms mean,p9535.282ms,cold101.789ms,32.134FPS; barrier consolidation has no clear whole-frame timing gain and does not meet p95 target. Full current regression pending.

**2026-10-17T03:48:48Z**

Full checkpoint0766758 regression before MPM control fix exited0:372reportedpasses,0failures,0VUIDs. Subsequent forced-schedule test confirmed ct-fmj6; full rerun after MPM changes remains required.

**2026-10-17T04:26:06Z**

Native640x360 physics AND display, scale1,90slots,pressure20,cadence6,2fixed60Hzsteps/frame, visible exact1176/1872pixel bodies now pass warm mean+p95 gates twice:26.419/26.683ms mean,30.672/30.660ms p95,37.852/37.477FPS; cold94.165/93.897ms remains. Dry4.516ms mean,p955.827,cold18.856,221.445FPS; dense17.131mean,p9525.866,cold31.188,58.373FPS. MPM3.56to1.24ms/tick. Actualbothconcave visuals pass, mass/holes/fourpausedframes preserved; RGB screenshot inspected. Do not close yet: full regression/UI/quarter acceptance and cold extraction remain.

**2026-10-17T04:58:42Z**

Sustained RED: opt-in 180-frame identical native640x360 physics/display fixture, 2 fixed60Hz ticks/frame, original quality and 33.333ms mean/p95 gates. Unprimed run mean30.056ms,p9538.394,max44.466,cold93.702,33.271FPS. Short30-frame pass is not sustained completion. Diagnostic repeat mean31.707,p9546.372; end2exact cached pixel bodies,10pairs,0contacts,6928pixelvisits/928boundaryA,hydro0.692ms,contacts8substeps2.768ms,other1.172ms. RED56e1346. Full headless010ef76 passed389 with0VUID; UI/quarter remain queued.

**2026-10-17T05:03:22Z**

Initialization-only trace isolates presentation watchdog failure to validation overhead:91 pipeline creations5223.643ms withvalidation versus36.796ms without. Unchanged15second three-context/270frame presentation test passes withoutvalidation; full test_ui then exits0. No runtime startup/Wayland change or gate relaxation. Full389 headless correctness remains validation-clean; full quarter functional suite now running. Persistent read-only tools/diagnostics/gpu_frequency.py added with2passing CPU tests; no sysfs writes.

**2026-10-17T05:12:02Z**

Displayed-frame diagnostic reproduces sustained p95 RED:180frames mean29.917ms,p9538.016,max44.564,33.426FPS. Runtime unchanged; hooks diagnostic-only, expanded query path validation-clean. Frame72 wall44.195ms:rigid28.649,contact16substeps21.552,allhydro1.470,fluid10.554,MPM2.591;0emittedcontacts. Frame73 rigid24.141/contact17.160/hydro1.530/0contacts. Late178 rigid18.711/contact11.941/hydro1.429/24contacts plusfluidphase3 14.359ms andphase2 4.651ms. Thus contact path andphase3 transport are measured next targets; phase0 wet detection~0.1ms/frame is not dominant. Read-only cold frequency sampler observes100MHz-to1150/1300MHz ramps, correlation only. Fullquarter168pass andUI17pass plus smokes complete; no gates loosened.

**2026-10-17T05:43:59Z**

Sustained native360 warm target now passes twice after ct-sqg7 batching:180frames,25.773/25.776ms mean,32.789/32.973ms p95,38.800/38.796FPS. Exact shapes, native physics/display,2fixed60Hz steps/frame,cadence6,pressure20 unchanged. Cold91.454/91.018ms still unoptimized; no unqualified completion. New typed dispatch ABI rejects stale binaries. Targeted exact contacts, work counters and explicit sync validation pass; full current checkpoint regression required.

**2026-10-17T07:37:17Z**

User requested the existing material-painting graphical sandbox, not a benchmark demo. Full validated make scheduler73691 paused between children after particle27/27PASS; GPU released to parent for sandbox. Persistent benchmark demo scope cancelled before runtime integration; already committed controls RED0ceab18 retained and completed as isolated2/2 host tests, with benchmark/default paths unchanged. No validation/benchmark GPU work resumes until visible sandbox closes and parent releases lease. Full regression remainder and UI/quarter/stage trace remain pending.

**2026-10-18T02:12:13Z**

Observed cold startup while validating ctr-9e4n: after shader changes, dc_gpu_create before the first dense_support test spent about four minutes at one CPU core and ~1.9 GB RSS compiling pipelines; warm runs proceed normally. Consider lazy/batched pipeline creation and persisted pipeline caching separately from per-frame optimization. Logs build/terrain_component_final_green.log and build/terrain_component_integrated_full_suite.log in renderer worktree.

**2026-10-23T17:27:32Z**

Dynamic-body integration saved-world replay (lighting branch3ff98eb) preserves original snapshot and now grows64->128 slots, reaching71 active bodies. Actual Wayland quarter display480x270/simulation640x448, lighting disabled, on copied saved level03: without Vulkan validation mean61.523ms worst66.540ms over10 frames; with core+sync validation mean148.744ms worst153.914ms over30 frames. Strict30ms benchmark gate correctly fails and has not been relaxed. Logs build/runtime_saved_scene_without_validation.log and build/runtime_final_saved_scene.log; native360 target remains unfinished in its separate optimization scope.

**2026-10-23T18:23:25Z**

Inline continuation: pushed dynamic-body foundation and verified optimization commits through 96e1ab3. Real Wayland saved-scene copies measure 67.621ms before optimization, 57.983ms with visible-only unlit rendering, 50.063ms after exact subgroup component statistics. Whole-line water scan regressed to55.286ms; replaced locally with adaptive register/subgroup batches: exact mass and RGBA in all16 direction/fallback comparisons under core+sync validation, real displayed mean46.991ms worst50.667ms. Target<30ms remains unmet and ticket stays in progress. Original world checksum unchanged. Profiling shows contacts~7ms, fluid~12-17ms, MPM~5.3ms; now evaluating immutable addressed root access and non-atomic post-union root traversal.

**2026-10-23T18:43:20Z**

Follow-up measurements: non-atomic post-union root traversal reduces roots~1.05ms to~0.08ms (validated cache/detach3/3). Read-only root annotation and stationary-water prefix experiments showed no improvement and were removed. Adaptive one-cell register batches preserve exact mass/pigment across24 cases, including packed long-reach water, with core+sync validation. A specialized local-size experiment disagreed even on the ordered path; replaced with literal16/32 compiled variants, exact24/24 under validation. Real displayed saved-scene mean47.285ms worst51.098ms (target remains RED). Contact bounds rejection10/10 validated; contact stage~6.7ms. Full make -i test running serially under core+sync to collect all failures, including known ctr-ojvy authored staging REDs.

**2026-10-23T19:18:43Z**

Contact regression work: exact packed boundary oracle4/4 fixtures reduces source reads47->7 without a material frame-time win. Small-pair tile-padding oracle RED278 candidates now86 with identical8 contacts (concave case64 contacts retained). Main cost was fixed16 workgroups/pair. Added GPU atomic compact work items with dynamic pair-capacity-sized metadata storage, fence-safe geometry/queue relocation, versioned dispatch ABI3 and indirect-publication barriers. Tiny pairs use1 group, empty terrain uses0 and still completes rejection accounting, large concave pairs use16 strided groups without dropping geometry. Focused core+sync contact8/8 plus dynamic growth/queue exhaustion/>64 fracture3/3 pass. Normal real Wayland saved scene60 frames44.443ms mean76.959ms worst, stable paused capture; budget still RED. Original saved world untouched. Full suite paused after passing body_carve7/7 to keep optimizing; final full suite must be rerun after final edits.

**2026-10-23T20:42:48Z**

Resolution audit: quarter-native visible physics and renderer are both480x270 (1:1 cells/pixels).640x448 is the rounded viewport plus a one-chunk offscreen simulation halo, not visible supersampling. Normal displayed saved-scene benchmark remains above30ms (~37.6ms best repeat); target not achieved. Continuing exact GPU optimization inline. New coalesced particle-copy byte oracle passes all five sizes under synchronization validation; integrating for displayed measurement.

**2026-10-23T21:08:55Z**

Inline progress: visible physics/render are1:1 at480x270;640x448 includes simulation halo. Explicit supported16-lane water subgroup control passes48 exact mass/pigment cases with synchronization validation;32 lanes and packed page caching did not improve displayed frame times, remain disabled. Solver motion publication now preserves every body/metadata byte while writing3-10 words instead of136 in repeated phases, including chunk crossing. Pixel-only buoyancy geometry retains raw type guards; wet3844-cell integration and14 buoyancy tests pass. Displayed saved-scene replay improved to35.643ms mean42.681ms worst, stable paused captures;30ms target still unmet. Testing removal of274 redundant intermediate host publications plus nested collision host publications, retaining final host visibility and compute/indirect barriers.

**2026-10-23T22:41:30Z**

Inline displayed saved-scene progress: reusable rigid command streams improved mean to32.408ms; MPM/fluid command streams now reused with exact CPU command-hash tests, parameter/resource invalidation and sync-validated loose-grain behavior. Hydrostatic transform reuse halves evaluations and preserves all exact heads (including721-row columns), displayed32.186ms. Compact GPU grain-bed work list preserves every support-graph float bit against dense reference,3/3 tests with synchronization validation, and improves real Wayland60-frame saved scene to30.702ms mean35.493ms worst. Four paused captures identical. Target<30ms remains unmet; now testing smaller contact solver workgroups without changing physics iterations/cadence/resolution or introducing body limits.

**2026-10-24T00:24:53Z**

Displayed GPU/API trace isolates warm delays to compute (not pipeline creation/allocation/acquire/present). Visible simulation/render are 480x270 1:1; 640x448 includes chunk-rounded offscreen simulation halo. Dirty extraction bounds/release costs ~1.5-2ms. Implementing sufficient-boundary-anchor proof to bypass already-rejected component statistics while retaining unknown-component diagnostics and original physical behavior. Normal displayed <30ms target remains unmet.

**2026-10-24T00:37:51Z**

Boundary-anchor bypass and narrowed indirect barriers pass component/cache4, extraction diagnostics3 and dynamic growth/fracture3 under explicit core+sync validation. Actual Wayland saved scene 90frames29.253ms mean33.765ms worst, repeat scalar baseline29.440/33.176; target remains RED. Fused grain/body apply removes72 dispatches and preserves every fixture byte; dynamic3/3 validation-clean, but180frame displayed29.692/34.222 shows no win, so DC_SOLVER_FUSED_GRAIN=1 remains opt-in. Original saved-world checksum unchanged.

**2026-10-24T00:52:41Z**

New default exact water optimizations: recompute unique recipient requests to release shared array;9 partial mass/pigment checks pass plus SPIR-V storage proof. Dry-line prescan reduces10240 cache cell loads to0, exact zero-water inputs preserved under core+sync. Actual Wayland180 frames25.357ms mean29.644ms worst, paused images stable. Longer360-frame repeat26.272ms mean131.020ms worst remains RED.400-frame coarse trace26.881/51.831; subsequent idle-stack trace sees37.851ms app frame with26.531ms present/GPU wrapper and no warm queue idle; not claiming sustained worst-frame completion. Full current make -j4 -i test under core+sync validation running. Benchmark label now distinguishes480x270 physics/render from640x448 active region including halo.
