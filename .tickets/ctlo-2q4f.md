---
id: ctlo-2q4f
status: in_progress
deps: [ctlo-ldb7]
links: [dun-sndp, dun-i1zd, ctfs-2hu1, ctlo-18l4]
created: 2026-10-24T22:06:18Z
type: feature
priority: 1
parent: ctlo-d7vm
tags: [fire, smoke, physics]
---
# Persist reactive state across terrain particles rigid pixels and coatings

Implement versioned owner-specific sidecars for thermal energy, fuel/char, water, coating composition, oxidation/corrosion, stable ignition-hazard state and bound oxidizer. Do not steal leaf color or loaded-terrain bits.

## Design

Follow design/fire_smoke_reactive_materials.md. Runtime implementation must use RED-GREEN-REFACTOR and repository tests/diagnostics; this turn creates the roadmap only.

## Acceptance Criteria

Exact state survives extraction, carving, fracture, MPM pool swaps, negative-coordinate streaming, save/reload and owner-generation changes. Atomic admission failure retains all source inventories; old saves preserve existing moisture/colors/ownership and initialize ambient reaction defaults. Corrupt/newer input rolls back. No fixed body/fire cap, per-frame CPU scan or hot-path allocation.


## Notes

**2026-10-25T00:40:18Z**

Progress through fb44d0b: 9 parcel/codec CPU tests, 2 Intel GPU ownership probes, and 4 native Wayland Vulkan ownership cases pass (16 displayed frames at480x270 physics/render with640x448 halo). Cell/shape/MPM handle storage is lazy and dynamically grows; extraction, carve bank staging, both MPM pool modes and leaf-bit handoff preserve state. The ticket remains in progress: impact/analytic paths, particle erase/reuse, GPU runtime admission and actual stream/checkpoint integration remain required. Buffer-address cleanup has passed the four displayed cases; full regression battery is running before its REFACTOR commit. No combustion/smoke milestone or final performance acceptance is claimed.

**2026-10-25T00:51:16Z**

Full regression RED found one new issue beyond the two tracked authored-stage failures: shape_color_resolve_tests reported pool/capacity queries6/6 instead of <=3/3. Fixed callbacks to guard before evaluating arguments and reuse the existing cooperative leader values; the unchanged Intel GPU test now reports exactly3/3,24 copy lanes and0 RGBA errors with core+sync validation. First complete battery had684 C passes and this regression plus the2 known failures. Native validation and full battery are being repeated before the cleanup commit.

**2026-10-25T00:57:58Z**

REFACTOR validation complete: corrected unchanged color-work test reports3 pool queries/3 capacity queries/24 lanes/0 color errors; all4 native Wayland ownership cases pass again. Repeated full make -j8 -i test has685 C passes plus6 fixture checks, only the same2 ctr-ojvy authored-stage failures at67/135, and0 compiler warnings/new failures. State/ownership ticket remains in progress; no final epic frame-time acceptance is claimed.

**2026-10-25T01:12:20Z**

Particle lifecycle GREEN:4 new RED cases reproduced stale state after air/repaint, lost secondary-promotion state, leaf RGB flags becoming soil moisture, and state loss across a negative-coordinate halo transfer. All4 now pass on native Wayland Vulkan at480x270+halo with core/sync validation (16 presented frames); the prior4 ownership cases pass again on Intel. Canonical A erasure retires only its entry and clears stale B handles without retiring unrelated B aliases. Halo runtime variant follows particle handles and promotion; material replacement clears incompatible flags. No ticket/epic closure yet.

**2026-10-25T01:29:25Z**

Generation GREEN: a new RED case reused hot slot0 and confirmed the runtime owner advanced to generation2 while its reactive record incorrectly stayed1. Extraction and carving now use the actual/pending runtime ownership generation, distinct from structural fracture lineage. All5 live ownership cases pass on native Wayland Vulkan with core+sync validation (20 presented frames). Continuing with GPU arena reservation/reuse/rollback and remaining persistence/handoff paths.

**2026-10-25T01:38:35Z**

GPU arena foundation GREEN:4 Intel core/sync-validated cases pass for freed-slot reuse/generation safety, all-or-nothing rollback under injected VkAllocateMemory failure followed by successful growth/retry, stale/duplicate retire rollback, and identity-preserving updates with stale peers. Allocation reserves entries on GPU and commits or rolls back the whole batch; output handles stay unchanged on failure. This isolated arena is not yet attached to live admission; integration and streaming/checkpoints remain required.

**2026-10-25T01:45:49Z**

Full-width arena addressing GREEN:256 Intel GPU arithmetic cases now match uint64 reference values, including carry and high index bits; the RED request-address stub had112 errors. Request storage uses BDA rather than a large storage-buffer descriptor, and all4 arena rollback/reuse/update cases still pass with core/sync validation. The first address harness attempt used the minimal probe default buffer and crashed its process; fixed its explicit8192-word allocation plus compile-time output bounds before the valid RED/GREEN runs. No computed test address is dereferenced. Live admission integration is next.

**2026-10-25T02:05:16Z**

Embedded packet codec GREEN:4 new CPU cases pass for canonical bytes identical to the existing FRS1 file codec, immutable input, transactional capacity/corruption/new-version/typed-owner rejection and empty ambient packets. The9 existing parcel/file-codec cases also pass with zero compiler warnings. These memory packets are ready to embed in atomic chunk/structure formats; actual format integration is still required.

**2026-10-25T02:19:31Z**

Live state storage now uses the GPU reserve/publish/rollback arena instead of CPU append-only entries. Native Wayland Vulkan integration passed six cell/body/MPM/reuse cases and four particle lifecycle cases, including 128 erase/repaint cycles with 64 live entries and no historical allocation growth. Persistence, physical admission and remaining handoff paths still in progress.

**2026-10-25T02:25:24Z**

Persistent identity GREEN: actual Wayland Vulkan regression moved a live leaf parcel, repainted its vacated cell and reproduced two concurrent particles sharing physical PID. Their owners now retain distinct 64-bit birth identities across movement; all5 lifecycle display cases pass (20 presented frames) with core/sync validation. FRS1 writes semantic ABI2 and reads ABI1 without accepting its reserved high-birth bits. CPU packet6 and state9 tests pass. Chunk attachment codec3 CPU cases pass; actual stream/checkpoint embedding remains underway.

**2026-10-25T02:33:23Z**

Atomic DCC7 streaming GREEN:3 new asynchronous worker cases plus7 unchanged stream cases pass. GPU chunk channel GREEN:3 actual Wayland Vulkan cases pass with core/sync validation and12 displayed frames. Save compaction builds sparse CELL/PARTICLE records on GPU; restore validates actual GPU materials/PIDs and rejects duplicate parcel IDs in other resident owners before allocation/publication. Replacing slot state retires old entries and retains the identity high-water mark. App-level wiring, DCS checkpoint integration and remaining mechanical/admission paths are still required.

**2026-10-25T02:40:02Z**

REFACTOR regression complete through e11f84f: corrected full make -j8 -i test reports712 C passes, only the same2 ctr-ojvy authored-stage failures at67/135, zero compiler warnings and no new failures. The first run exposed a missing standard object rule for reactive_chunk_gpu; fixed the rule, discarded that incomplete run, and repeated the complete battery. Added allocator/address/chunk CPU+GPU/worker cases to the standard recipe. Native displayed lifecycle5 and GPU chunk3 cases passed separately with core/sync validation.

**2026-10-25T02:41:48Z**

Level-view stream wiring GREEN: a native Wayland Vulkan RED exercised actual schedule_saves/process_results and found result.reactive absent. The app now exports GPU channels into the immutable worker save and restores them before finishing chunk publication. GREEN presents4 frames at480x270+halo and retains exact bound water, heat, coating and ignition history after worker reload. DCS structure-checkpoint integration remains separate and unfinished.

**2026-10-25T02:45:05Z**

Chunk slot reuse GREEN: native RED confirmed a new physical chunk inherited the previous chunk CELL heat/oil. Validated cold uploads now preflight and retire the old CELL/PARTICLE handles in parallel before mutating physical data; allocation/resource failure leaves the old upload source intact. All4 GPU chunk cases and the actual level-view worker reload case pass on Wayland Vulkan with core/sync validation (20 presented frames). Next: DCS body and pending-chunk channels.

**2026-10-25T02:47:38Z**

DCS checkpoint RED confirmed exact body and pending-negative-chunk state are currently lost on reload (2 native Vulkan cases fail). Shared FRW1 archive codec now passes3 CPU cases for one global identity counter across body/chunk channels, cross-channel duplicate identity/counter rollback rejection, empty high-water persistence and corrupt/future/truncated rollback. This codec alone does not complete DCS integration; both graphical checkpoint cases remain RED until the transaction is wired.

**2026-10-25T03:02:22Z**

Safe boundary after modular user steering: private detached-storage factory and parallel BODY snapshot compaction pass2 native Wayland cases; all4 chunk cases and actual level worker reload case pass again (28 presented frames total, core/sync validation). Exact unlinked DCS helper/header/private hooks preserved as design/drafts/reactive_checkpoint_integration.patch; git apply --check passes. No DCS8 format enabled. Both explicit checkpoint display tests remain intentional RED and outside the passing default battery. Remaining broad scope: DCS body/pending checkpoint persistence and rollback, other impact/analytic handoffs, runtime physical admission and loose-cell/particle transitions. These are deferred until required by the active visible feature; this ticket is NOT complete. Next visible sequence is ctfs-2hu1 heat/wetting, then ctfs-dmxv smoke, then ctfs-81vw terrain bonfire.
