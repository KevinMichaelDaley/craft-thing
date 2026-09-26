# Eulerian water with sparse virtual interface markers

## Source of truth

Water volume is a `uint32_t` Q16.16 fill value (`0..65536`) at each fixed world
cell. A marker is massless: it must never become a water particle, store water
volume, or directly add or delete cell volume. Summing cell fill values is the
authoritative mass check. This keeps saves, chunk streaming, and interactions
with sand and rigid occupancy on the chunk grid.

The grid solver uses a conservative, pairwise finite-volume flux method. Two GPU
mass layers ping-pong for each active resident cell. Each of four substeps pairs
disjoint neighbors: vertical even, vertical odd, horizontal even, horizontal
odd. Both cells read the same old layer, choose one bounded face flux, and write
their own new values to the other layer. The sender loses exactly the amount
the receiver gains. No atomics or in-place write races are needed. The maximum
flux per pair is capped at half a cell per substep. A persistent grid face
velocity supplies the requested flux, while source volume and destination
capacity determine the accepted amount.

Before transport, a GPU predictor damps old face velocity and adds gravity on
open downward faces. A cell-centered pressure field then solves the discrete
Poisson equation using 20 red-black SOR sweeps with relaxation 1.5; each color
is a separate dispatch and a Vulkan barrier separates colors. Liquid cells
carry pressure, air at the free surface has zero pressure, and solid, occupied,
or unloaded neighbors close the face. Subtracting the pressure gradient from
predicted face velocity reduces divergence. Q16.16 cell fill remains the
conserved volume field; liquid density is constant, so there is no separate
density solve. The current fixed sweep count is validated for a small closed
basin, but a measured residual or multilevel method is needed for larger
liquid regions.

Solid material and final rigid occupancy close a face. A missing neighbor page
also closes the face for this substep; mass remains in the source cell. The
current solver reads resident neighbors through the page table rather than a
separate halo refresh. At the end of the
fluid stage, the final mass layer is written to the chunk atlas and dirty slots
are recorded for persistence. The sand stage reads only this finalized state.

## Sparse markers

Once conservative grid motion works, seed two massless 16.16 fixed-point
markers per surface cell, one just inside and one just outside the water
interface. Add at most two more where curvature or stretching warrants it.
Markers exist only in a narrow band around the free surface and use a bounded
GPU pool keyed by world chunk, with stable IDs and deterministic seeding.
Advect them with interpolated grid face velocities, clip against solid/rigid
boundaries, transfer ownership across resident chunks, and freeze or serialize
them when a chunk sleeps. Reconstruct an interface guide from markers and grid
fill. Escaped markers may request a local anti-diffusive transfer between
neighboring cells, but the same paired, equal-and-opposite flux resolver must
accept that request. Markers can sharpen an under-resolved surface; they cannot
violate the volume budget. Render markers only in a debug overlay.

Sparse markers target visible interface smearing. They do not, by themselves,
guarantee mass conservation or preserve bulk kinetic energy. Integer flux
accounting guarantees volume; the pressure solve does not prevent numerical
damping, which must be measured in benchmarks. This follows the distinction between conservative
volume-of-fluid tracking and massless particle-level-set interface correction.

## Verification and staged delivery

1. Make painted water visibly fall and spread through the fixed-cell flux
solver. Check exact volume over 100+ ticks in a closed basin, no flow through
stone or unloaded pages, and equal results across a resident chunk seam.
2. Add GPU-generated interface markers and a readback/debug overlay. Check
bounded count, deterministic replay, advection with the accepted face flux,
and marker ownership across chunk edges.
3. Add marker-guided conservative interface correction. Compare a thin-sheet
and splash scene with markers enabled/disabled; require a sharper interface
without any change in total water volume. Save/reload moving water and marker
state after chunk eviction.
4. Benchmark dense fluid and sparse splash scenes at the planned internal and
native resolutions. Report per-pass GPU timestamps, active slots, marker
count, volume error, and frame pacing before changing the default scale.

References: Rider and Kothe, *Reconstructing Volume Tracking* (1998), and
Enright et al., *A Hybrid Particle Level Set Method for Improved Interface
Capturing* (2002/2003). The marker count above is a deliberately smaller
project budget than the published particle-level-set examples and must be
validated by the thin-feature tests.
