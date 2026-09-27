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
the measured MPM stage from roughly 100–120 ms to single-digit milliseconds
before component labeling on the test device. With labeling, the 384×256
resident-window test currently measures about 10–15 ms under test load; actual
timing varies with content and GPU load.

The same active-tile list drives a 4-connected component pass for dirt. A cell
starts with its global viewport index as a label; adjacent component roots
hook toward the smaller root with an atomic minimum, then path compression
rewrites labels. A GPU-written indirect dispatch command stops further rounds
when a hook round makes no change. The count pass records each component's
size at its root, including dirt across resident chunk seams. There is no
per-frame readback. The label and size buffers can serve future rigid-body
extraction, though that feature needs a separate material selector and body
construction pass. The maximum round count grows with the viewport cell
count; the 384×256 window allows eighteen rounds, although converged scenes
stop dispatching the hook and compression work sooner.

The first pass gathers particle contributions onto grid nodes through the
linear tent kernel `w = max(0, 1-|dx|) max(0, 1-|dy|)`. It sums fixed-record
mass and momentum in a fixed source-cell order, avoiding floating atomic
contention and giving repeatable results. Each particle's deformation `F`
provides `J = det(F)`. The compressive pressure is
`k max(0, 1-clamp(J, 0.5, 1.5))`, with `k = 5` for sand, `3` for dirt, and `8`
for gravel. A capped symmetric shear term supplies limited grain resistance;
there is no tensile stress. The grid solve divides momentum and stress force
by mass, adds gravity, scales velocity by 0.999 per half-tick substep, and
projects velocity against stone cells and the GPU
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
explicit overshoot for the two half-tick substeps. This feedback exchange
uses no atomics or per-frame CPU readback. Granular volume and local water fraction also
produce a buoyancy force on the grains, with the opposite impulse written to
the water feedback field; dense sand and gravel still sink.

Before the two MPM substeps, a GPU cell-owner pass exchanges free Eulerian
water with granular-particle moisture. Each dirt particle can bind at most
0.25 cell of water and each sand particle 0.0625 cell. One tick absorbs at
most 0.0625 cell for dirt or 0.015625 cell for sand, debiting the cell by
exactly the credited 16.16 amount. With no free water in its cell, a particle
returns up to 64 fixed-point units per tick to the Eulerian cell. Both particle
slots are processed in stable order by one invocation, avoiding competing
writes. A dirt component of at least eight connected cells becomes mud at
2048 bound units and returns to dry dirt at or below 1024; smaller dirt
components have low shear resistance and wet pressure that separates them.
Sand retains pore water and gets a wet color when uncovered.
Bound moisture travels with the particle ID through movement, chunk seams,
and save/reload; it is never counted as a second free-water cell or marker.
Wet dirt has lower compressive bulk and shear resistance and wider plastic
strain limits than dry dirt. The renderer colors mud separately when no free
water covers its pixel. Painting water onto granular material retains its
particle and painting granular material into water retains the Eulerian mass.

G2P interpolates grid velocity (PIC), updates `F` with the grid velocity
gradient, caps strain for plastic yielding, and proposes a new position. Sand,
dirt, and gravel carry effective 16.16 contact radii of 0.25, 0.375, and 0.5
cell while each particle still represents one cell of material mass. Each
particle reads at most eighteen neighboring slots across the page table and
adds pairwise size-dependent contact and friction impulses to its grid
velocity. Overlapping grains receive separating impulses; close unlike-size
grains exchange an equal-and-opposite vertical kinetic-sieving impulse so
smaller grains percolate into lower gaps while larger grains are displaced
upward. The pair impulse is weighted by both particle masses, and the
fixed 3×3 search and velocity cap bound work and motion. Material density
also enters the buoyancy response, so submerged grains settle according to
their volume. No screen-cell exchange, CPU particle sort, or copyback is used.
The next pass ranks competing arrivals by stable particle ID. Each cell owns two
fixed GPU slots; arrivals can use only vacancies present at the start of the
substep. Rejected arrivals stay in their source cell with zero velocity. A
destination-cell gather sorts accepted particles by ID into the two slots,
and a commit pass updates material cells and per-chunk particle counts. This
policy conserves each particle's fixed-point mass and makes overflow explicit:
a full cell blocks entry without losing a particle. Page-table neighbor reads
cross chunk seams directly, serving as a zero-copy halo view. Missing pages
block transfer until streaming activates them.

Chunk files now use version 5 so grain radius is persistent and can be changed
per particle. Version 4 particle records are migrated from their former shared
0.5-cell radius to the material defaults on load; earlier versions still seed
particles from material cells.

The solver neither downloads particles nor rebuilds its buffers per frame.
GPU timestamps for the sand stage measure the complete MPM work; the headless
test also checks exact mass, stable IDs, deterministic replay, collision with
stone and a rigid body, coupled wet-grain motion and seam crossing. A closed
uniform-flow test checks each water/grain drag exchange to within four 16.16
fixed-point units after accounting for intentional velocity damping; still
water does not carry grains sideways. The one-second
UI smoke produces
`build/screenshots/granular_before.bmp` and
`build/screenshots/granular_after_1s.bmp`.
The coupled one-second UI smoke paints a supported dirt surface over the
generated stone, then water, dirt, sand, gravel, an isolated fragment, and an
enclosed nine-cell mud component. It checks the visible floor, mud and
fragment classifications, and moving grains, and saves
`build/screenshots/coupled_before.bmp` and
`build/screenshots/coupled_after_1s.bmp`.
The sifting demo starts a 63-grain mixed column inside a stone chamber on dry
generated terrain. `--smoke-sifting` captures
`build/screenshots/sifting_before.bmp` and
`build/screenshots/sifting_after_1s.bmp`, then checks material counts, walls,
and the mean sand/gravel depth gap after 60 GPU ticks. `--demo-sifting` leaves
the same scene interactive.
