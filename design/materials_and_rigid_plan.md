# GPU material coupling and multi-body plan

The new work is tracked by epics `dun-9det` (granular materials and geology),
`dun-i1zd` (wood), and `dun-5kye` (rigid contacts). It extends the current
rigid -> Eulerian water -> material-stage tick graph. All physical state and
normal-frame updates stay on GPU; readback remains opt-in for tests,
diagnostics, screenshots, and chunk eviction saves.

## Implemented rigid baseline (`dun-0p7p`)

The GPU pool holds 64 stable-ID boxes. Fixed-step integration
updates each transform independently, and terrain supports falling boxes.
Spawning an existing ID updates only that body; removal permits slot reuse.
Each raster pass writes current occupancy and a separate conservative swept
AABB spanning the previous and current transforms. The lowest nonzero ID wins
mask overlaps. Fluid and rendering consume current occupancy; the swept mask
is available for contact diagnostics without blocking water along old trails.
Empty scenes skip rigid dispatches, and removing the last body clears its masks
on the next tick. Normal frames do not read transforms back to the CPU.

Seven Vulkan regressions cover capacity, updates, removal, deterministic masks,
independent falling, and horizontal/vertical chunk seams. The quarter-native
window test verifies that successive `B` spawns retain both visible boxes.
Broadphase and body contacts remain `dun-4ftd` and `dun-x9ei`. Swept occupancy
does not yet resolve contacts.

## World ownership and persistence (`dun-rwls`)

Each body owns a signed 64-bit chunk anchor and canonical local 16.16 position.
The shader carries movement across chunk seams and derives a viewport transform
from the current simulation origin. Chunk coordinates use pairs of 32-bit words
with checked carry/subtraction, so the runtime does not require shaderInt64.
Camera rebasing refreshes masks without advancing physical time. GPU body slots
remain independent of terrain atlas slots and their generations: chunk eviction,
slot reuse, and reload cannot change body identity or redirect its position.
Bodies outside the resident viewport sleep with their world pose and velocity
preserved; masks clip to resident cells. The bounded pool holds 64 bodies across
the whole loaded world, including sleepers.

Closing a world saves all active and sleeping bodies to `rigid_bodies.bin` via a
temporary file and rename. Reopening restores IDs, anchors, local poses, sizes,
and velocities before rendering. Version 1 uses a 16-byte header and 48-byte
native-endian records, consistent with the existing native-endian chunk files.
The loader validates capacity, canonical positions, IDs, duplicate IDs, version,
and complete length before replacing the pool. Snapshot reads occur only during
world save and explicit tests; ordinary physics, seam ownership changes, and
camera transforms stay on GPU. These rectangular bodies still use the baseline
downward terrain contact rather than the upcoming world broadphase/XPBD solver.

## State ownership and tick order

- Water volume stays in fixed Eulerian cells. Pressure, face velocity, and
  conservative flux remain the water source of truth. Sparse interface markers
  remain massless.
- Dirt, sand, gravel, and suspended sediment live in bounded, chunk-owned MPM
  particle pools. Each particle has a stable ID, world position, velocity,
  solid mass, grain properties, deformation state, and bound moisture. A GPU
  P2G/grid/G2P sequence runs after the fluid pass. Particle occupancy is
  rasterized to pixels for drawing and material contacts, without replacing
  particle state with a screen-space rule.
- Stone and wood pieces large enough to behave as bodies live in world-space
  rigid-body buffers. Their convex geometry and material properties determine
  contact, buoyancy, fracture, and rot. Final body occupancy is rasterized
  before fluid transport. The rigid pass samples the previous completed water
  state for buoyancy; the fluid pass sees the newly resolved body occupancy.
- A separate chunked wear map records slow stone weathering. Wood exposure and
  rot state belong to the body and survive chunk streaming. Simulated time is
  explicit so month/year tests can accelerate the clock without changing the
  normal fixed-step interpretation.

## Conservative exchange

Transfers between Eulerian water, particle moisture, stationary terrain,
sediment particles, and rigid fragments are GPU transactions. A conversion
debits its source and credits its destination exactly once. Water volume is
never stored as a second independent MPM liquid pool. Momentum exchanged by
water drag uses equal-and-opposite impulses; the resulting source enters the
next water projection. Pool saturation must defer or reject a conversion while
retaining the source, never silently discard mass.

The rigid broadphase emits bounded world-space AABB candidates. Convex-to-
pixel narrowphase emits contacts against one-cell terrain and convex bodies.
Parallel Jacobi XPBD accumulates corrections per body in separate buffers and
applies them together each iteration. Contact work and fracture thresholds
feed stone breakup; newborn gravel enters the particle pool, while larger
fragments become new rigid bodies.

## End-to-end gates

Each implementation ticket requires a headless state/budget test and a
windowed scene once the behavior is visible. Test sequences include chunk
seams and save/reload, mixed material painting, dry and wet controls, gentle
and hard stone drops, floating and sinking bodies, and accelerated long-time
wear/rot. Validate Vulkan synchronization and record GPU stage timing at the
same stage as the feature; capacity and memory traffic are part of the
implementation, not postponed to the resolution benchmark.
