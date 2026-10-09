---
id: dun-9qub
status: in_progress
deps: [dun-ci2x]
links: [dun-mi1e]
created: 2026-09-26T07:43:36Z
type: task
priority: 2
assignee: kmd
parent: dun-5kye
tags: [gpu, rigid, xpbd]
---
# Solve rigid contacts with parallel Jacobi XPBD on GPU

Apply terrain and body contacts using parallel Jacobi XPBD iterations.

## Design

Accumulate positional and rotational corrections in separate per-body buffers, then apply simultaneously each iteration; use compliance, mass/inertia, friction, restitution, warm-start policy, and bounded substeps. Update velocity from corrected poses and publish final occupancy before fluid/MPM.

## Acceptance Criteria

A multi-body stack settles without race-dependent penetration or energy blow-up; moving support and hard drops behave reproducibly; GPU stage order and timing are validated, with no per-frame CPU contact solve or readback.


## Notes

**2026-10-05T05:13:34Z**

dun-ci2x now supplies GPU-owned contacts through binding 4. dc_gpu_contact_word_offset locates the header after all broadphase records; header is 12 words, contact records are 128 bytes. Six public counters precede an indirect command at words 6..8. Consumers must gate ALL corrections on nonzero contact overflow (capacity=1, world=2, incomplete broadphase=4). Normal points B-to-A; body pairs have sorted IDs, terrain/boundary/MPM have body_b=0. Anchors are canonical signed world chunk plus local 16.16; feature_chunk identifies a terrain/MPM cell independently of where its witness lies. Material namespaces differ for bodies and cells. High-bit body feature denotes an edge. Surface coefficients are prototypes; derive physical mass/inertia and density from material properties. Preserve previous poses and use bounded substeps for fast/thin bodies because narrowphase is discrete. The existing timestamp includes contact generation; no ordinary-frame counter/transform readback.

**2026-10-08T06:59:33Z**

GPU implementation is committed through 78a2af6, following RED/GREEN/REFACTOR. Binding 4 now has 64 x 232-byte Body records, unchanged broadphase/contact sections, then a 16-word solver header, 32 words/body scratch, and 16 words/contact scratch. Twelve specialization phases use GPU indirect contact dispatch. Eight substeps and eight Jacobi iterations use deterministic signed fixed-point reductions; runtime rotation is about material-derived COM, with area/mass/inertia/density computed on GPU (stone 2.7, wood 0.6 relative to water=1). Compliance, Coulomb friction, restitution, and cold-started per-substep multipliers are active. Any incomplete contact set rolls the entire tick back. Final exact polygon occupancy and conservative swept coverage precede fluid/MPM. V3 snapshots preserve angular state/flags and still accept V1/V2.

Validation on this machine's Intel Vulkan ICD: make test passed all 132 headless cases under Vulkan validation; full 640x448 quarter-native solver passed 9 cases and quarter-native fluid solver passed 29. SPIR-V validation passed rigid, solver, broadphase, contacts, and tick probe. Dense quarter_native_bench passed the unchanged >=60 Hz gates: 15.660 ms/tick (63.86 Hz), 16.352 ms/tick+render (61.15 Hz). This dense fixture has no active rigid bodies; combined dense rigid/fluid profiling remains covered by dun-2b08.

Do not close yet: the added quarter-native rendered stone/wood stack, its camera eviction/reload assertions, and the existing window/UI checks need an unlocked desktop. The RED window scene compiled and failed on the deliberately stubbed diagnostic API. The diagnostic implementation is now present, but the GREEN render run cannot finish: ScreenSaver.GetActive=true, and Wayland withholds the first frame callback. Bounded attempts reproduced the wait without Vulkan validation errors. No lock/compositor settings were changed. After the desktop is unlocked, run build/quarter_native_rigid_tests (or --solver-only first), make test_quarter_native, and make test_ui; fix any actual failures, commit the GREEN window result, and close this ticket only after those checks pass. Streamed asleep-slot contact handling is noted on dun-x9ei for its follow-up acceptance work.

**2026-10-09T02:41:46Z**

Unlocked-desktop GREEN validation: the rendered quarter-native stone/wood stack passes settling, runtime material density, camera eviction, and exact snapshot reload. Corrected a signed/unsigned expected-coordinate calculation in the test (negative world Y was converted to unsigned); the physical state already settled correctly. make test_quarter_native passed config 2, cadence 2, fluid 29, window 6, full-grid rigid solver 9, and native presentation smoke. On Intel Vulkan with validation, presentation measured 60.23 physics ticks/s and adaptive 61.3 frames/s. UI and final headless checks remain before closure.
