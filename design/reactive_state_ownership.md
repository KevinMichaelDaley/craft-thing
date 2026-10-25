# Reactive parcel ownership foundation

Ticket: `ctlo-2q4f`, still open. This document records the implemented
foundation and its boundaries; it does not declare the fire/smoke epic complete.

## Persistent representation

`fr_reactive_state_t` occupies 28 little-endian 32-bit words in snapshots.
Owner keys retain ABI 2 semantics; current parcel ABI 3 adds compensated heat
in two formerly reserved words without changing the 28-word width. ABI 1/2
saves with zero reserved words remain readable. Particle
owners include a full-width lifetime identity, separate from their physical PID.
Repainting a vacated cell can reuse that PID while the original grain is alive.
The birth identity remains stable during movement and distinguishes both parcels.
Nine Q16.16 fields retain dry reference mass, fuel, char, inert material, bound
water, oil/jelly coating fuel and core/coating oxidizer. Dry reference mass is
metadata, excluded from the physical species sum. Absolute thermal energy and
pending shock energy are extensive; oxidation, corrosion and both integrated
ignition hazards retain their history. Stable parcel and origin IDs are separate.

Colors remain in their existing channels. Soil/sand bound water mirrors the
existing moisture flags and is counted once. Leaf pigment flags are never read
as water. The CPU reference split assigns integer residuals exactly and divides
energy by the children's heat capacities without rerolling ignition thresholds.

`FRS1` is a cold, transactional codec for logical owner/state records and the
next parcel ID. It checks versions, lengths, hash, reserved fields, finite values,
duplicate owners and parcel IDs. The next ID must exceed parcel **and origin**
IDs. Insufficient capacity or corrupt input does not publish partial output;
missing old-save data initializes zero nonambient records.

The `FRC1` chunk channel combines canonical logical records with serialized
physical row bindings. These are chunk-local cell/particle rows, never GPU arena
indices or addresses. Row bindings disambiguate equal physical particle PIDs on
restore. The worker validates material, identity, held-terrain role, unique rows,
state and counters against the immutable physical chunk before publishing.
`DCC1` version 7 stores this channel, physics and colors in one temporary/rename
transaction with a whole-file checksum. Older chunk versions remain readable
with ambient reaction defaults; empty channels still retain the ID high-water
mark. The level view and structure checkpoint wiring remain to be completed.

## GPU storage and completed paths

The context allocates no reactive buffers until explicitly admitted state exists.
A small stable BDA root is published in reserved body work words 64–66. Dynamic
entry storage uses 36 words per entry: generation, six logical owner words,
padding and the 28 state words. Handles use a full-width index and generation;
there is no packed index quota or fixed global body/fire count. Entry and shape
bank growth copies payloads on the GPU before publishing new addresses. The live
pool now uses the GPU arena's reserve/publish/rollback protocol, reuses retired
entries, and never resets saturated generations. Requests use full-width BDA
address arithmetic rather than a storage-buffer descriptor range or packed index.

Cell handles are separate from cell flags. Shape handles follow both geometry
banks. Particle handles follow the physical A/B pools, including the default
two-substep alternation and the diagnostic copy mode. The root survives dynamic
body work replacement; shape handle banks are preflighted before geometry growth.

Implemented mechanical handoffs:

- Terrain extraction moves the existing handle to the committed body texel,
  clears its cell handle and records body identity/generation/rest coordinates.
- Geometry bank copies retain exact state; staged carve copies mask empty texels
  so two child shapes cannot claim the same surviving parcel.
- Committed carving updates surviving owners and retires erased solid parcels.
  Saturated generations are retired rather than wrapped into valid old handles.
- Leaf crumbling moves the same parcel into a committed leaf-bit particle,
  changes only its material identity and owner, and preserves its composition,
  heat, coatings and hazards. Missing destination handle storage prevents leaf
  reservation, retaining the source. Erased leaves and emitted leaves differ.
- MPM gather carries the chosen source handle with each chosen particle and
  clears empty destinations. Copy commits copy handles; alternating commits
  retain the A/B orientation without copying the particle pool.
- Air/material replacement retires canonical particle state; secondary promotion
  and negative-coordinate halo movement retain state and pigment. Incompatible
  leaf pigment bits do not become soil moisture after a material replacement.
- Slot save compaction constructs the sparse CELL/PARTICLE list in parallel on
  GPU. Cold restore validates actual GPU materials/PIDs and checks new IDs against
  other resident entries before reserving the entire batch and replacing handles.
  Conflicts leave both original inventories intact. Packet processing by the
  disk worker is separate from runtime component/reaction work.

`fr_gpu_reactive_seed_*` APIs are explicit **cold fixture/import** boundaries.
Their CPU validation, resource growth and diagnostic reads are not a runtime
reaction loop or a CPU component solver. New IDs advance a high-water mark;
moving material never calls these APIs. Gameplay admission must remain on GPU.

## Evidence and remaining work

The CPU material tests (9), parcel/codec tests (9) and locked fixture tests (6)
pass. The GPU transfer probe checks exact state bits and generation safety on
the actual Intel Iris Xe. Native Wayland validation at `fb44d0b` passed all four
live ownership cases with Vulkan core and synchronization validation, presenting
16 frames at 480×270 physics/render with the 640×448 halo. The preceding three
cases also passed a separate displayed run with MPM particle copying enabled.
These are functional checks, not the final complete-frame performance gate.

The address-load cleanup and its query-count correction passed the same four
native displayed cases again. The unchanged staged-color work test reports
three pool queries, three capacity queries, 24 copied texels and zero color
errors. A first full battery exposed its accidental 3→6 query regression; the
fix guards callback arguments and reuses existing cooperative leader values.
The repeated full `make -j8 -i test` battery reports **685 C passes and the same
two tracked authored-stage failures** (`ctr-ojvy`, lines 67 and 135), plus six
fixture checks, with zero compiler warnings and no new failures. Logs remain in
`build/reactive_ownership_full_suite_fixed.log` and
`build/reactive_query_fix_display.log`. The cold pre-correction four-case native
invocation took 215.63 seconds including compiler startup; no startup performance
improvement is claimed by the buffer-address cleanup.

Through `e11f84f`, the extended battery (including the arena, full-width address,
chunk codec/worker and GPU stream tests) reports **712 C passes**, the same two
tracked authored-stage failures, and zero compiler warnings or new failures.
The first attempt exposed a missing standard object rule for the new GPU chunk
module; that incomplete run is not validation. The corrected full run is
`build/reactive_stream_full_suite_fixed.log`. Separate actual-display runs pass
all five particle lifecycle cases and all three GPU chunk save/restore cases
with Vulkan core and synchronization validation, totaling 32 presented frames.

Before closing this ticket: finish the other impact-fracture/analytic-body
handoffs, GPU runtime admission and actual level-view/structure checkpoint
integration. Add corresponding RED tests,
complete the full regression battery and retain the final epic's two 1000-frame
displayed performance runs for each of its three scenarios. Heat transport,
combustion and smoke are not enabled by this foundation alone.

## Modular handoff boundary

The user's delivery sequence now advances to `ctfs-2hu1`, the interactive
heat/wetting bench, before more horizontal persistence work. The final safe
boundary reports **718 C passes and the same two tracked authored-stage
failures**; there are zero compiler warnings/new failures in
`build/reactive_safe_boundary_full_suite.log`. Native Vulkan core/sync validation
also passes two body-snapshot cases, four chunk-channel cases and the actual
level-view worker save/reload case (28 presented frames at 480×270 with halo).

The detached resource factory and GPU BODY compaction are tested helpers, not
an integrated structure checkpoint. The two explicit checkpoint display cases
remain intentional RED outside the default passing battery. The exact unlinked
checkpoint preparation draft and private hooks are preserved in
`design/drafts/reactive_checkpoint_integration.patch`, accompanied by its status
note; `git apply --check` succeeds. No DCS8 format was enabled. This ticket stays
open for its remaining scope and must not block a visible feature on unrelated
future owner/material paths. Bring forward only prerequisites that the active
feature actually needs.

The subsequent visible heat/wetting module needed a scoped integrated save path.
DCS8 now frames logical reaction state with liquid enthalpy and the finite probe,
prepares detached resources and all pending handles before publication, and
passes native body/pending/rollback/reopen tests. DCC8 preserves liquid heat
through actual camera eviction/reload. The old draft above is historical and
must not be reapplied; see its updated status note and
`design/heat_wetting_bench.md`. The default regression now reports 740 C passes,
the same two `ctr-ojvy` failures and zero compiler warnings/errors. This does not
close the remaining impact-fracture, analytic-owner and general admission scope
of `ctlo-2q4f`.
