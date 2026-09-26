# GPU material coupling and multi-body plan

The new work is tracked by epics `dun-9det` (granular materials and geology),
`dun-i1zd` (wood), and `dun-5kye` (rigid contacts). It extends the current
rigid -> Eulerian water -> material-stage tick graph. All physical state and
normal-frame updates stay on GPU; readback remains opt-in for tests,
diagnostics, screenshots, and chunk eviction saves.

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
