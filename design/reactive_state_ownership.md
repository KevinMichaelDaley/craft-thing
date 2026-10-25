# Reactive parcel ownership foundation

Ticket: `ctlo-2q4f`, still in progress. This document records the implemented
foundation and its boundaries; it does not declare the fire/smoke epic complete.

## Persistent representation

`fr_reactive_state_t` is ABI 1, 28 little-endian 32-bit words in snapshots.
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
missing old-save data initializes zero nonambient records. The codec currently
exists independently of the world's chunk and structure checkpoint formats.

## GPU storage and completed paths

The context allocates no reactive buffers until explicitly admitted state exists.
A small stable BDA root is published in reserved body work words 64–66. Dynamic
entry storage uses 36 words per entry: generation, six logical owner words,
padding and the 28 state words. Handles use a full-width index and generation;
there is no packed index quota or fixed global body/fire count. Entry and shape
bank growth copies payloads on the GPU before publishing new addresses.

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

Before closing this ticket: finish the other impact-fracture/analytic-body
handoffs, particle erasure/reuse paths, transactional runtime admission and actual
chunk streaming/structure checkpoint integration. Add corresponding RED tests,
complete the full regression battery and retain the final epic's two 1000-frame
displayed performance runs for each of its three scenarios. Heat transport,
combustion and smoke are not enabled by this foundation alone.
