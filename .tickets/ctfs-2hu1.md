---
id: ctfs-2hu1
status: in_progress
deps: [ctlo-ck1i, ctlo-ldb7]
links: [ctlo-2q4f, ctlo-18l4]
created: 2026-10-25T02:54:23Z
type: feature
priority: 1
parent: ctlo-d7vm
tags: [fire, smoke, modular, gameplay]
---
# Deliver an interactive heat and wetting bench as the first visible system

User steering: implement one visually testable system/feature at a time. After stabilizing the current persistence checkpoint, deliver the smallest usable thermal prerequisite to a terrain bonfire. Reuse completed material/parcel work; do not finish every owner/checkpoint path first.

## Design

See the modular delivery section of design/fire_smoke_reactive_materials.md. This user-authorized sequence takes precedence over completing broad horizontal foundation tickets first.

## Acceptance Criteria

RED-GREEN-REFACTOR. A permanent placement-demo entry or deterministic scene lets the player inject finite heat, heat adjacent wood/stone, apply water and inspect temperature/wetness with clear overlays. Actual heat propagates locally; water cools/wets and energy/water budgets close. Pause freezes state; reset and existing supported persistence/terrain edits do not leak or duplicate state. Demonstrate dry/wet controls on actual Wayland Vulkan with screenshots and complete-frame measurements, preserving 1:1 grid and mechanical cadence. One coherent thermal module/API; no fake flame or completion claim for convection, gas, arbitrary-body burning or all of ctlo-18l4. This usable visible system must be committed/pushed and reported before smoke work.


## Notes

**2026-10-25T02:56:00Z**

Next active visible system after the current safe checkpoint. Reuse completed parcel/profile/streaming work; bring forward additional ownership or save support only when an exposed interaction needs it. A scoped demonstration must not claim unsupported persistence or suppress existing mechanics.

**2026-10-25T03:20:14Z**

Thermal reference GREEN:6 RED tests now pass for composition-derived capacity, signed conservative face flux, four-face monotonicity, finite probe reservoir, exact Q16 wetting mass and proportional enthalpy, preserved identities/hazards, pause and invalid-input rollback. Existing parcel9 tests also pass; no compiler warnings. Bench sources are finite350K reservoirs (not output clamps), keeping this first module below boiling by construction. Next is the real Vulkan thermal field and displayed placement-demo interaction; CPU reference alone is not milestone completion.

**2026-10-25T03:36:46Z**

First Vulkan thermal field GREEN: both native Wayland cases pass with core/sync validation (8 presented frames). GPU face exchange matches CPU energy within1e-4, paused presentation leaves parcel bytes unchanged, and the real8x8 rigid/2-substep MPM tick applies one thermal interval. Displayed framebuffer capture: build/screenshots/thermal_reference_pair.bmp (converted PNG inspected). This is a two-cell functional field check, NOT the interactive bench or completed milestone. Water/admission/bench controls and full-frame acceptance remain underway.

**2026-10-25T04:08:34Z**

Interactive prototype GREEN: native placement bench case passes finite probe heating, actual dry/pre-wet inventories, overlay switching and4 identical paused frames; the2 core Vulkan field/clock checks pass again. Permanent --heat-bench entry uses existing placement controls,4-worker immutable scene construction and real shader feedback. Native dependency RED caught stale thermal SPIR-V; fixed native objects to depend on runtime module shaders. Inspected real temperature/wetness and normal CLI captures. Performance remains FAILED:120frames10.808mean/36.530worst;180 traced10.574/45.921, largest input20.553+present25.342. Free-water coupling, inspection, edits/admission, restore and passing frame gate are still required. Do not close from this prototype.

**2026-10-25T04:31:43Z**

Water stage RED6af74f9 now locally GREEN: actual Wayland/core+sync2 cases pass for exact16-quanta wetting, CPU enthalpy reference, moving liquid excess-energy conservation and custom pigment. Separate thermal volume channel follows existing fan-out and subgroup prefix paths. Runtime liquid fixture corrected to canonical AIR substrate plus fluid_mass; assertions unchanged. Existing thermal/fluid regressions underway. No milestone completion or frame-gate pass claimed.

**2026-10-25T04:35:07Z**

Water GREEN regression:20 existing cases pass across native thermal2, native interactive bench1, transport10, subgroup scan1 (56 internal direction/variant cases), pigment4 and partial-recipient2. Core+sync logs contain no validation errors after enabling BDA in the minimal fluid fixture and binding zero thermal control. New displayed water2 also pass. Startup tracing is opt-in DC_PIPELINE_TRACE; no temporary tools. Full module acceptance remains pending.

**2026-10-25T04:38:05Z**

Edit-boundary REDc31e592 exposed stale liquid heat both after AIR erase and physical chunk replacement. GREEN now4/4 native Wayland/core+sync water cases pass: painting ambient water preserves existing heat while adding ambient mass, erasure removes proportional heat, fresh water cannot resurrect it, and validated completed physical-slot replacement clears its old liquid channel. Heat/removal remains an explicit external authoring sink; chunk persistence still pending.

**2026-10-25T04:47:45Z**

Liquid-stage REFACTOR/full regression boundary e8e22ea: make -j8 -i test finished718 C passes and exactly the two known ctr-ojvy authored-stage failures, zero compiler warnings. Log build/thermal_water_full_suite.log. Native liquid4 and existing thermal/fluid20 cases passed separately with core/sync validation. New wood/stone admission RED source is queued, not yet run/claimed.

**2026-10-25T04:51:59Z**

Admission REDb12f850 now locally GREEN3/3 actual Wayland/core+sync: newly painted wood matches canonical ambient payload and finite CPU probe reference, replacement stone gets fresh identity/ambient heat with one live record, and613-cell brushes grow beyond the64-entry initial allocation and recycle all entries across5 erase/repaint cycles. Dynamic preflight reads allocation counters only; GPU constructs/publishes state. Scope wood/stone bench solids; compatibility checks underway, broad owner admission still open.

**2026-10-25T04:57:10Z**

Admission compatibility complete:16 existing cases pass (arena4, ownership6, brush1, displayed liquid4, displayed bench1) with core/sync validation and no errors; new admission3 also pass, build zero warnings. The prior718+2 full log is the liquid-stage boundary, not a claim that the newly changed admission source already passed a second full run. Next visible work is inspection/labels and water interaction capture before any broader prerequisite expansion.

**2026-10-25T05:10:54Z**

Feedback RED42c67e8 now GREEN: permanent make test_heat_bench passes18 total (6 CPU reference +12 actual displayed Vulkan), zero core/sync validation errors. GPU hover numbers match solid/liquid inventory and change with target while pause preserves exact payload/water heat; actual captures inspected (water317.1K/100%, dry probed wood296.0K/0%). Renderer adapter is separate and solver no longer binds framebuffer. Fixture saturation amount corrected from mistaken0.35 assumption to actual wood moisture_capacity0.5;100% expectation/tolerances unchanged. Visual cases added to test_ui/test_quarter_native, reference6 to test. Next water-pour scene capture and fresh graphical timings; module still open.

**2026-10-25T05:20:25Z**

Actual placement-pour integration passes alongside original bench case (2/2 native/core+sync, no warnings/errors). Radius3 water brush above warmed wood injects29 units; next physical interval retains exact1900544 Q16 water, energy discrepancy-0.000042149 vs locked0.02 absolute tolerance, wood295.968906K ->295.936791K with16 bound quanta and unchanged identity. After60 further physical steps actual capture shows294.7K/3.8%wetness and water flowing into basin. Added acceptance coverage uses existing GREEN kernels, no new runtime implementation required. Cold fixture fixed to supply logical chunk coords to export API. Next normal graphical full-frame measurements.

**2026-10-25T05:31:03Z**

Permanent tools/bench_heat_bench now completed four real Wayland Vulkan1000-frame trials with no concurrent builds/GPU work: dry13.771mean15.309worst; dry13.712/15.252; finite-pour14.183/15.553; finite-pour14.163/16.039ms. All strictmean ANDworst<30, all4paused capturesidentical; eachpour exactlyonceatframe8 viarealplacementAPI. Evidence build/heat_bench_measurements_aaT6PO and build/thermal_repeated_display_benchmark.log. Earlier currentdry13.612/17.986 and13.644/15.788also pass; oldprototypefailedtails remainrecorded. InputRED7fe932b ->3GREENCPUchecks; permanenttool/reusableheaderinrepo. Resolution/cadenceunchanged, lightingoff. Heatbenchperformance passes current dry/pour cases; ticket staysopen for requiredownership/edit/save. This does not certifyfuture3epicscenarios.

**2026-10-25T05:39:02Z**

Pre-persistence REFACTOR safe boundary35cf70c: full default suite727 C passes, exactly2 known ctr-ojvy authored-stage failures, zero warnings (build/thermal_feedback_full_suite.log). Actual dry/pour1000x2 performance passes remain recorded. Heatbench is still open because supported restore currently refuses worlds; audit preserved DCS transaction draft now only for this visible module needed state, including liquid enthalpy and finite probe. No DCS8 active yet, no future material/owner admission claimed.

**2026-10-25T05:45:51Z**

Persistence REDb7ff0d9 compiled and failed2/2 native tests. Minimal source diagnostic now passes its real-contact/atomic-output case; rerun reaches the actual missing behavior: save/load succeeds physically, but restored thermal source is absent (1pass/1fail, build/thermal_checkpoint_source_display.log). This confirms the old checkpoint omission before format integration. Existing two reactive checkpoint REDs remain separate from default battery. No DCS8 linked/enabled yet.

**2026-10-25T05:53:01Z**

Thermal codec RED507fed8 ->6GREEN CPU cases, zero warnings. THR1 stores exact signed liquid excess heat, logical negative-coordinate bindings, finite source and world-purpose metadata in versioned little-endian48-byte framing. It validates ambient/water-capacity references and profile ABI before publication, rejects orphan/negative-absolute-energy/NaN heat, duplicate chunks, checksum corruption, future versions and truncation/trailing bytes; output unchanged on failure. Codec added to default tests. Gameplay DCS restore remains RED/unlinked while integration proceeds; no completion claim.

**2026-10-25T07:00:02Z**

Displayed persistence integration: native transaction4/4, extracted-body/pending negative chunk2/2, frontend bench4/4 pass under core/synchronization validation. Exact payload/source/liquid restoration into another slot, future-version rejection and14 injected allocation-failure rollbacks preserve newer live state. Extended placement pour exposed tiny orphan heat from floating reciprocal full withdrawal; RED04b035b -> GREEN948ed17 fixes exact full-inventory transfer,60 further physical steps pass per-step orphan checks and strict save. Pushed948ed17. DCS8 integration remains uncommitted pending legacy/full regression; next scoped exposed gap is liquid enthalpy in camera-streamed chunk packets. Broad ctlo-2q4f stays open.

**2026-10-25T07:09:08Z**

Scoped camera streaming prerequisite now GREEN locally: DCC8 stores immutable liquid enthalpy with physics/colors/FRW packet and outer checksum; chunk THR record has no probe inventory, global DCS remains its sole source authority. Worker3/3 pass invalid zero-mass/nonfinite/negative-full-enthalpy rejection, immutable mixed channels, corruption/future rejection and old ambient compatibility; existing reactive3 and world7 stream cases pass. Real Wayland camera RED5c73ba7 now passes exact liquid bytes after20-chunk eviction/reload with identical finite source; actual inspected capture thermal_bench_stream_resume.png shows296.1K/100% water. Prior native4+2+frontend4 and legacy structural store8/streaming5 all pass. Full regression and displayed timing repeat next.

**2026-10-25T07:38:51Z**

Extended conservation REDd38658a found true61-step energy drift-0.027087608 outside locked0.02, despite exact water mass. A conductive-quantization diagnostic still failed-0.024460072 and was removed. Scoped compensated parcel accumulation now gives-0.000357833 with exact1900544 mass; tolerance unchanged. ABI3 uses two former reserved words for normalized heat remainder/correction without growing28-word state; ABI1/2 zero-reserved migration is retained and malformed legacy bits rejected. CPU precision4/thermal6/state9/packet6 pass, including100000 sub-ULP additions error-2.52e-10 and split/serialization. Native pour1/core2/water4/checkpoint4 pass core+sync; checkpoint includes nonzero remainder and14 allocation rollbacks. The previously recorded736+2 full run predates this precision fix; full/default/native and displayed timings are being repeated before closure.
