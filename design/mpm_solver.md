# GPU granular MPM stage

The tick order is rigid occupancy, Eulerian water, then granular MPM. Every MPM
tick has two equal substeps with `dt = 0.5` tick. Particle speed is bounded to
two cells per tick, so a particle moves at most one cell per substep. This is
the ownership CFL bound used by the 3×3 source search; it also bounds the
explicit stress update. The six granular passes and one water feedback pass
of each substep run in the existing
Vulkan command buffer, with compute barriers between them.

A GPU activity scan marks 16×16 tiles containing granular material, then a
GPU compaction pass adds a one-tile neighbor band and writes an indirect
dispatch list. Every MPM substep dispatches only those tiles. The list is
rebuilt each tick from GPU-owned cells and counts; there is no CPU decision or
copyback. The neighbor band covers grid interpolation and the maximum two-cell
motion over both substeps. On the 384×256 resident-window test this reduced
the measured MPM stage from roughly 100–120 ms to roughly 5–8 ms on the test
device; actual timing varies with content and GPU load.

The first pass gathers particle contributions onto grid nodes through the
linear tent kernel `w = max(0, 1-|dx|) max(0, 1-|dy|)`. It sums fixed-record
mass and momentum in a fixed source-cell order, avoiding floating atomic
contention and giving repeatable results. Each particle's deformation `F`
provides `J = det(F)`. The compressive pressure is
`k max(0, 1-clamp(J, 0.5, 1.5))`, with `k = 5` for sand, `3` for dirt, and `8`
for gravel. A capped symmetric shear term supplies limited grain resistance;
there is no tensile stress. The grid solve divides momentum and stress force
by mass, adds gravity, and projects velocity against stone cells and the GPU
rigid occupancy map. Water and grain can occupy the same cell: granular
material is permeable to the Eulerian flux and projection passes, while stone
and rigid occupancy remain solid boundaries.

Each grid node samples the projected water face velocities and the average
Eulerian mass on each face. For each axis, the drag impulse is
`0.5 * (grain_mass * water_face_mass / (grain_mass + water_face_mass)) *
(water_face_velocity - grain_velocity)`. The grid velocity receives the
impulse divided by grain mass. The opposite velocity change is stored in the
grid force buffer's unused components and applied by a separate, uniquely
owned face pass after a compute barrier. Zero-water faces produce no drag or
feedback. The second MPM substep samples the updated water velocity; the next
fluid prediction and pressure projection consume it as well. The drag
coefficient is bounded below one and the reduced-mass form prevents an
explicit overshoot for the two half-tick substeps. This uses no atomics,
per-frame CPU readback, or new persistent buffer.

Before the two MPM substeps, a GPU cell-owner pass exchanges free Eulerian
water with dirt-particle moisture. Each dirt particle can bind at most 0.25
cell of water; one tick absorbs at most 0.0625 cell, debiting the cell by
exactly the credited 16.16 amount. With no free water in its cell, a particle
returns up to 64 fixed-point units per tick to the Eulerian cell. Both particle
slots are processed in stable order by one invocation, avoiding competing
writes. Mud enters at 2048 bound units and returns to dry dirt only at or
below 1024, so a small fluctuation cannot switch the material each tick.
Bound moisture travels with the particle ID through movement, chunk seams,
and save/reload; it is never counted as a second free-water cell or marker.
Wet dirt has lower compressive bulk and shear resistance and wider plastic
strain limits than dry dirt. The renderer colors mud separately when no free
water covers its pixel. Painting water onto granular material retains its
particle and painting granular material into water retains the Eulerian mass.

G2P interpolates grid velocity (PIC), updates `F` with the grid velocity
gradient, caps strain for plastic yielding, and proposes a new position. The
next pass ranks competing arrivals by stable particle ID. Each cell owns two
fixed GPU slots; arrivals can use only vacancies present at the start of the
substep. Rejected arrivals stay in their source cell with zero velocity. A
destination-cell gather sorts accepted particles by ID into the two slots,
and a commit pass updates material cells and per-chunk particle counts. This
policy conserves each particle's fixed-point mass and makes overflow explicit:
a full cell blocks entry without losing a particle. Page-table neighbor reads
cross chunk seams directly, serving as a zero-copy halo view. Missing pages
block transfer until streaming activates them.

The solver neither downloads particles nor rebuilds its buffers per frame.
GPU timestamps for the sand stage measure the complete MPM work; the headless
test also checks exact mass, stable IDs, deterministic replay, collision with
stone and a rigid body, coupled wet-grain motion and seam crossing. A closed
uniform-flow test checks equal-and-opposite x momentum to within four 16.16
fixed-point units; still water does not carry grains sideways. The one-second
UI smoke produces
`build/screenshots/granular_before.bmp` and
`build/screenshots/granular_after_1s.bmp`.
The coupled one-second UI smoke paints water, dirt, sand, and gravel in the
procedural level, checks a wet dirt particle and moving grains, and saves
`build/screenshots/coupled_before.bmp` and
`build/screenshots/coupled_after_1s.bmp`.
