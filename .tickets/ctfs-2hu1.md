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
