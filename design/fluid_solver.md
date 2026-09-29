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

Before transport, a GPU predictor backtraces vertical face velocity through the
previous velocity field, adds gravity on open downward faces, and diffuses both
face-velocity components with a kinematic-viscosity coefficient of 0.03 cell²
per world tick. A snapshot of the previous face field makes the four-neighbor
Laplacian race-free without another GPU dispatch. At a solid wall, the missing
neighbor uses the opposite tangential velocity as a no-slip ghost value; at a
dry open face it uses the same value for a zero-shear free surface. An adjacent
solid also adds 0.05 wall friction per world tick. A missing workspace page
uses zero shear rather than masquerading as a stone wall. The explicit viscosity
coefficient is capped at 0.12 for longer offscreen steps. Viscosity precedes
the pressure solve so its divergence is projected out. The predictor initializes
pressure from the local hydrostatic water-column head. A cell-centered pressure
field then solves the discrete
Poisson equation using 20 red-black SOR sweeps. Columns shallower than 13 cells
use relaxation 1.5. Deeper columns in chunks without MPM particles use 1.9 to
converge long-wavelength pressure modes without additional dispatches; each color
is a separate dispatch and a Vulkan barrier separates colors. Liquid cells
carry pressure, air at the free surface has zero pressure, and solid, occupied,
or unloaded neighbors close the face. Subtracting the pressure gradient from
predicted face velocity reduces divergence. Q16.16 cell fill remains the
conserved volume field; liquid density is constant, so there is no separate
density solve. The current fixed sweep count is validated for a small closed
basin, but a measured residual or multilevel method is needed for larger
liquid regions. A two-chunk, 32-cell-high river regression displaces the
surface, saves rendered frames before and after one and four seconds, and
checks exact mass, high spray, and late vertical speed. The deep-column rule
reduced mean absolute vertical speed after four seconds from 0.0518 to 0.0218
cells per tick while retaining the split-versus-monolithic seam result.

Solid material and final rigid occupancy close a face. A missing neighbor page
also closes the face for this substep; mass remains in the source cell. The
current solver reads resident neighbors through the page table rather than a
separate halo refresh. At the end of the
fluid stage, the final mass layer is written to the chunk atlas and dirty slots
are recorded for persistence. The final face-velocity writeback retains 0.997
of velocity per world tick in visible workspaces and 0.92 offscreen, without
changing the mass budget or the marker comparison within that tick. The
granular stage reads this finalized state.

## Sparse markers

The first marker pass seeds two massless 16.16 fixed-point markers at each
selected surface site, one just inside and one just outside. Cells below one-eighth
fill are not seeded, avoiding a new inside marker in every diffuse fringe.
Selection keeps the first eligible site in each 2 × 2 cell block, so a dense
interface cannot request more than the 2048-entry chunk pool at seeding time.
Markers live
in two bounded 2048-entry GPU buffers per world chunk; GPU compaction moves
them between resident chunk slots, and sleeping slots retain their state.
Each marker has a stable hash ID derived from its original chunk and local
cell. Face-aligned bilinear velocity sampling advects markers, and blocked destinations
clip them to their old position. Every resident slot is reset and compacted each
update, and markers beyond the wet/dry interface are discarded. A GPU per-cell guide records inside/outside
presence. Separate disjoint vertical and horizontal pair passes can move at
most 8192 Q16.16 units from an outside-only cell into a neighboring
inside-marked cell with at least as much water. Correction also runs at moving
interfaces. Each pair applies equal-and-opposite updates,
so markers cannot add water mass. The `M` overlay displays marker guides.

Chunk files persist the active marker count, records, and per-cell face velocity
alongside grid cells; version 1 files load with an empty marker pool, and
versions 1 and 2 load with zero saved velocity. The fixed pool is deliberately
bounded: excessive advected markers crossing into one chunk can still saturate
it, so the marker pass caps counts and preserves the authoritative grid volume.
The current tests
show sharpening on controlled thin-sheet and two-lobe splash edges. Broader
free-surface reconstruction remains a benchmark target, not a claim that all
thin features survive arbitrary flow.

Sparse markers target visible interface smearing. They do not, by themselves,
guarantee mass conservation or preserve bulk kinetic energy. Integer flux
accounting guarantees volume; the pressure solve does not prevent numerical
damping, which must be measured in benchmarks. This follows the distinction between conservative
volume-of-fluid tracking and massless particle-level-set interface correction.

## Verification and staged delivery

1. Make painted water visibly fall and spread through the fixed-cell flux
solver. Check exact volume over 100+ ticks in a closed basin, no flow through
stone or unloaded pages, and equal results across a resident chunk seam.
2. GPU-generated interface markers have a bounded count, deterministic
single-step replay, a debug overlay, and resident chunk-edge ownership.
3. Marker-guided conservative correction has on/off thin-sheet and splash
captures with higher measured concentration at equal total water volume.
The window smoke verifies marker save/reload after chunk eviction.
4. Benchmark dense fluid and sparse splash scenes at the planned internal and
native resolutions. Report per-pass GPU timestamps, active slots, marker
count, volume error, and frame pacing before changing the default scale.

References: Rider and Kothe, *Reconstructing Volume Tracking* (1998), and
Enright et al., *A Hybrid Particle Level Set Method for Improved Interface
Capturing* (2002/2003). The marker count above is a deliberately smaller
project budget than the published particle-level-set examples and must be
validated by the thin-feature tests.
