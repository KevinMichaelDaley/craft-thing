---
id: ctr-ebl5
status: in_progress
deps: []
links: []
created: 2026-10-19T01:33:39Z
type: task
priority: 1
parent: ctr-6fii
tags: [renderer, vulkan, lighting, performance]
---
# Remove water-induced light shafts and optimize propagated lighting

User reports that water creates light shafts. Fix that artifact while optimizing the actual directional radiance transport noninteractively in a separate worktree. Preserve partial leaf transmission, diagonal skylight, lateral ambient fill and underground attenuation.

## Acceptance Criteria

RED-GREEN-REFACTOR regressions demonstrate that water neither emits light nor introduces spurious light shafts. Preserve canopy shadow recovery, opaque/unknown blocking, colored moments and stable paused frames. Benchmark Vulkan on this machine at 480x270 and 640x360 with unchanged physics and report isolated GPU timings versus the 32.186ms renderer baseline. Acceptance is total simulation plus rendering frame time <30ms on this machine, at both 480x270 and 640x360 with unchanged physics fidelity; renderer stage times are diagnostic only. Validate the actual graphical application and inspect captured real frames alongside core/synchronization validation and headless tests. Work autonomously without user questions. Commit repository tools and tests, update tickets, run full headless suite and push.


## Notes

**2026-10-19T01:34:04Z**

USER CORRECTION: acceptance is TOTAL FRAME TIME <30ms on this machine, including simulation and rendering, not a2ms renderer-only budget. This supersedes the2ms target in the original acceptance. Benchmark the complete frame at current scene configuration and640x360; preserve simulation fidelity and light transport correctness. Work noninteractively.

**2026-10-19T01:51:23Z**

Water regression verified RED for false 0.08 bulk-water extinction and for wet solids losing opacity. GREEN clear-water optics: zero solid occlusion and no emission; wet stone remains opaque; terrain/moving leaves retain partial opacity. Compact FP32-energy, signed10-bit direction-ratio transport reduces per-cell intermediate storage256 to120 bytes plus unchanged64-byte final field. Specialized phases, source/reach prepass,32-lane subgroup scans with validated shared fallback. Lighting11/11 and scene1/1 pass core+sync. Full-frame benchmark now measures actual tick + render + submissions/waits at480x270 and640x360,16 pressure sweeps unchanged. Current contended means46.969ms/76.511ms (renderer27.582/50.682; light20.917/39.803), so <30ms acceptance remains unmet and ticket stays in progress. Existing demo PID288414 untouched; i915 /proc fdinfo measured93.3% render-engine occupancy over7.61s. No isolated acceptance claim. Full headless suite pending.

**2026-10-19T02:00:46Z**

Final validation: full headless make -j4 -i test completes with exactly the2 pre-existing ctr-ojvy authored_stage_tests failures at67/135 and zero new failures/warnings (build/opt_full_test.log). Extended lighting regression12/12 passes core+sync, including recursive scan carries across x1024; shared fallback12/12 also passes. Clear/wet water, terrain/body leaves, immutable paused frame/physics scene1/1 passes core+sync (opt_last_lighting.log,opt_final_fallback.log,opt_last_scene.log). Final strict full-frame benchmark at unchanged16 pressure sweeps fails, as required:480x270 mean46.516ms/worst51.501ms;640x360 mean77.207ms/worst93.399ms. Renderer/light26.930/20.056ms and49.179/38.762ms. Timings overlap existing demo PID288414 and are NOT isolated acceptance; no demo launched, stopped or edited. Repository benchmark has cached native object compilation, dependency tracking, optional per-pass timestamps and full simulation+render mean+worst <30ms gates. Optical correction also lets the GPU dirty gate reuse lighting while only clear water flows, because water mass no longer changes opacity. Remaining full-frame performance/isolated verification stays in progress, tracked here and ctr-21eh. Branch feat/lighting-frame-optimization pushed; final checkpoint follows.

**2026-10-19T03:15:16Z**

Resumed actual graphical benchmarking. Surface28 exact RGB commit085a2ad and final field24 d7247eb pass core+sync lighting12/12 and scene1/1. Uncontended direct480 fullframe20.104mean/34.233worst,64033.536mean/52.177worst; targetstillRED, continuing. Actual640 halo coupledscene142.816mean/180.047worst with paused pixels identical; visible directional bands require further visual work. No performance completion claimed.

**2026-10-19T22:10:55Z**

Inline takeover checkpoint: dry scheduled-fluid gate and GPU grain/body work planner reduce actual level_piece03 640x360 graphical mean from about67ms to20.878ms; worst62.580ms remains RED. These are unchanged physics cadence/resolution, no hot readback. Height-basis transport candidate passed correctness but same-binary A/B showed no material frame-time benefit (640 means29.040 vs29.015ms); candidate discarded. Testing compact diagonal work scheduling next. Full suite currently running; performance target not met and work remains in progress.

**2026-10-19T22:15:09Z**

Compact diagonal schedule verified RED work-count, GREEN13/13 subgroup and shared fallback with core+sync validation. Immutable cold geometry table omits absent scan tiles; parent scans explicitly use identity for absent totals. Strict480 mean16.473/worst38.475ms,640 mean25.831/worst41.067ms (unchanged16 pressure sweeps); target remainsRED. Actual Wayland level03 640display/768x512sim mean20.748/worst64.560ms90frames, four paused captures identical; inspected build/inline_diagonal360/scene.png. Full suite before this candidate completed with only two known authored-stage failures plus its intentional RED work-count failure. Final shader broad suite rerun pending.

**2026-10-19T22:20:26Z**

Graphical water diagnosis: plain --demo-coupled uses the legacy/albedo view, so its53.534ms mean is physics-only diagnostic evidence, not lit-renderer acceptance. Combining --scene level_piece03 with --demo-coupled enables actual propagated lighting:136.998ms mean/157.663ms worst. Diagnostic phase-specialized fluid preserves generic fingerprint993637574fbe0c8f and mass230613072, and plain water demo mean49.570ms (worst77.659). Preparing a production phase specialization regression before promotion. Startup empty-program preparation candidate was discarded after failing to remove first-frame spikes reliably.

**2026-10-20T17:03:07Z**

Resumed inline. Rejected removing internal HOST visibility after no reliable measured win. Narrow pigment sync port verified corrected RED zero-barrier assertion (10192 conservative barriers), GREEN8/8 reference hash/mass checks core+sync; standard water-color4/4 core+sync. Cached one-cell pigment barriers10192->0, mixed16986->6794, exact hashes retained both directions. Actual lit-water graphical107.875ms mean/126.091ms worst, four paused pixels identical; target still unmet. Testing zero-input subgroup scan skip for opaque/unlit regions next.

**2026-10-20T17:04:49Z**

Zero-input subgroup scan skip passed13/13 core+sync but graphical107.801ms mean was indistinguishable from107.875ms baseline; discarded. Full retained-code make test rerun is underway (build/resume_full_test.log). No resolution, pressure-sweep, transport-reach, or physics-cadence reductions; no timing samples excluded. Parent target remains RED.

**2026-10-20T17:33:09Z**

Retained direct8-byte optical-cell transport and removed unused per-frame hierarchy generation. Lighting14/14 subgroup/shared core+sync and scene1/1 core+sync; renderer diagnostics now read authoritative optics. Strict640mean22.967/worst43.153ms, actual lit-water98.868/122.473ms. Common-direction transport subsequently verified RED16-byte working stride, GREEN16/16 subgroup/shared core+sync plus scene1/1; exact mode-switch roundtrip and general-field parity. Working energy/transmission stay FP32; common-source vertical moments derive analytically, mixed heights use20-byte general beams. Same-binary graphical common-direction86.005/102.675ms versus forced-general101.455/127.904ms; paused frames identical and actual capture inspected. Target remains RED.

**2026-10-23T14:38:18Z**

User-directed aggregate light buffer implemented: removed per-pixel emission/reach light-list loops. Parallel bounded source injection writes FP32 RGB/packed directional seed buffer with atomic union reach and deterministic colocated-emitter aggregation; propagation consumes the buffer. Source revisions retain seeds across authoritative optical edits. RED bounded-work and persistence tests observed failing, GREEN lighting 19/19 core+sync and shared fallback; scene validation and graphical recheck in progress. Initial uncached graphical lit-water 97.706ms mean/188.786ms worst, four paused captures identical and actual frame inspected. Strict 480 15.782/33.741ms,640 24.840/50.592ms; target remains unmet. Displayed-frame profiler identifies fluid39-43ms and renderer22-24ms, no fidelity reductions.

**2026-10-23T14:42:07Z**

Aggregate-buffer checkpoint verified: source persistence RED failed extra-build assertion, GREEN19/19 subgroup and shared fallback core+sync; final scene1/1 core+sync. Actual cached Wayland lit-water60frames86.481ms mean/102.980ms worst, four paused captures identical; inspected build/light_buffer_cached_graphical/scene.png. Full make -j4 -i test completed (build/light_buffer_full_suite.log), exactly two known ctr-ojvy authored_stage failures67/135 and zero new failures/warnings. Parent target remains RED. Next measured displacement work is ctlo-6tk8.
