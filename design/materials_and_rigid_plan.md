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
Broadphase is implemented below; body contact generation and solving remain
`dun-ci2x` and `dun-9qub`. Swept occupancy does not yet resolve contacts.

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
Version 2 adds 72 bytes of convex geometry and material to each record, for
120 bytes total. Version 3 adds angular state and flags (136 bytes); version 4
adds compound pieces (432 bytes). The loader accepts all four versions.
The loader validates capacity, canonical positions, IDs, duplicate IDs, version,
and complete length before replacing the pool. Snapshot reads occur only during
world save and explicit tests; ordinary physics, seam ownership changes, and
camera transforms stay on GPU. App contexts enable the GPU XPBD solver; the
box-only support approximation remains for legacy diagnostic contexts.

## GPU broadphase (`dun-4ftd`)

After integration and occupancy, the rigid stage clears bounded chunk buckets,
bins conservative swept AABBs with a one-cell contact margin, and emits body/body
and body/terrain candidates. Each bucket holds a 64-body bitset; the lowest
shared bucket owns a body pair, so overlaps spanning several chunks emit once.
Body IDs are sorted in body-pair keys. Terrain keys use body ID and signed world
chunk coordinate, with a boundary classification for missing or outer-neighbor
pages. Terrain candidates deliberately include empty chunks; narrowphase will
inspect actual cells and resolve blocking boundaries.

The data follows the 64 body records in storage binding 4, preserving the
32-buffer descriptor limit on this machine. It uses an eight-word header,
two words per bucket (including one outer chunk ring), and 4,096 bounded 32-byte
candidate records. Quarter-native adds about 129 KiB of storage. GPU counters
report stored and required counts; overflow flag `0x1` means candidate capacity
was exceeded and `0x2` means a world chunk key exceeded the signed coordinate range.
Any nonzero overflow makes the candidate set incomplete and must gate later
contact solving. Candidate ordering is unspecified; stable keys define identity.

No normal frame downloads candidates or counters. The existing opt-in rigid
timestamp includes broadphase dispatches, and explicit readback exposes keys and
counts for tests. Eight Vulkan regressions cover 2,016 dense body pairs, supported
motion, crossing trajectories, stale-pair clearing, world keys after camera and
slot changes, cold boundaries, bounded overflow, and signed outer-neighbor keys.
The quarter-native window test preserves all 28 pairs in an eight-body overlap
after camera eviction and reload. Contact generation remains `dun-ci2x` and the
solver remains `dun-9qub`; broadphase candidates alone do not resolve collisions.

## Convex shape storage and occupancy (`ct-l35t`)

The original convex shape storage added a 72-byte shape to the 80-byte transform
prefix. Current GPU records occupy 528 bytes including angular state, cached
bounds, and compound pieces. Shape vertices use body-local 16.16 coordinates;
authored rotation may be baked into vertices, and runtime rotation is about
the derived center of mass. Three through eight
counterclockwise vertices must form a strictly convex polygon within the body's
at-most-16-by-16 bounds. Material identifies stone or wood independently of
terrain cell material IDs. Invalid geometry leaves the pool unchanged. The box
spawn API clears custom geometry, including when reusing an existing ID or slot.

The Vulkan raster tests each polygon edge and the cell's two axes for positive
area overlap. A polygon can occupy a cell without covering its center; exact
edge-only touching does not occupy that cell. Fixed-point subtraction occurs
before conversion to local floating-point coordinates. Swept occupancy and
broadphase retain conservative box bounds, including fractional edge cells.
Only cells within the bounding box run polygon edge tests. The extra storage
is 4.5 KiB for all 64 shapes and consumes no additional descriptor.

World rebases and snapshot reloads retain geometry and material. Six headless
Vulkan tests cover rotated stone/wood shapes across a seam, thin one-cell
overlaps, atomic validation, far-world rebasing, version 1/2 persistence, and
slot reuse. The quarter-native window test renders both polygons, evicts and
reloads their terrain chunks, and reopens the saved session. Rendering still
uses the existing green body diagnostic color for both materials.

With the solver enabled, polygons and compound pieces use terrain/body contacts,
angular response, mass/inertia, compliance, friction, and restitution. The
convex storage prerequisite itself remains separate from the later solver work.

## Convex-to-cell narrowphase (`dun-ci2x`)

After broadphase, the GPU compares convex polygons using separating axes from
both sets of edges. Legacy boxes use four vertices and the stone body material.
Terrain and MPM contacts compare the body against occupied one-cell squares,
including thin overlaps that do not cover a cell center. Air and Eulerian water
do not produce solid contacts. Granular cells produce the MPM contact kind only
when at least one of their two particle layers has nonzero mass. Missing pages,
the outer chunk ring, and cells beyond a partial viewport tile produce blocking
boundary contacts. Terrain work clips the scan to the body's bounds and candidate
chunk instead of scanning every cell in every chunk.

Each convex piece-pair contact uses the minimum translation direction and support faces
of the two polygons. Containment uses directional translation distances rather
than only interval-intersection width. The normal points from B to A; applying
a positive correction to A along the normal separates it from B. Touching pairs
can have zero depth; the tolerance is one 16.16 unit. Contact generation is
discrete at each of eight solver substeps. Swept broadphase is conservative;
continuous time-of-impact collision detection is not implemented.

Each 128-byte contact contains sorted body IDs for body pairs, contact kind,
feature IDs, material IDs, normal, depth, friction, restitution, compliance, and
two world witness anchors. Each anchor uses a signed 64-bit chunk pair plus
canonical local 16.16 coordinates. Support-face witnesses use a shared tangent
coordinate when their face spans overlap. Body features encode vertex indices
or an edge index with the high bit set; bits 28–29 identify the compound piece
(zero for legacy shapes). Cell features use the local cell index
and a separate signed world feature chunk; a witness point crossing a chunk edge
does not change the cell's identity. Body and cell material IDs use separate
namespaces, selected by contact kind. Coefficients are current prototype values;
the solver consumes them along with material-derived mass and inertia.

The bounded contact region follows all 4,096 broadphase records in storage
binding 4. A 12-word header holds six diagnostic counters and a GPU-generated
indirect dispatch command. Up to 8,192 contacts add 1 MiB of storage. GPU-produced
candidate counts select the workgroup count, with no host count readback. Empty
body scenes continue to skip rigid dispatches. The rigid timestamp includes
contact generation and no additional storage descriptor is required.

Contact overflow flags are capacity `0x1`, unrepresentable world anchor `0x2`,
and incomplete broadphase `0x4`. An incomplete broadphase clears the contact
count and prevents generation from its truncated candidates. Any nonzero contact
overflow gates the XPBD solver and rolls back its tick; capacity overflow retains only
the diagnostic prefix. Required counts describe representable contacts before
the output limit. Output order is unspecified; stable feature keys identify
contacts. Explicit diagnostic readback validates caller capacity before touching
the outputs. Normal frames leave counters and contacts on the GPU.

Nine headless Vulkan regressions cover rotated stone/wood on stepped/sloped
single-cell terrain, thin edge overlap, false contacts in empty polygon corners
and water, all three MPM materials, body-pair symmetry, world anchors after camera
and slot changes, bounded overflow and broadphase gating, cold pages, and partial
viewport tiles. The quarter-native window scene preserves a rotated stone/wood
pair's complete contact record across camera eviction and session reopen.
Contact generation is complete; collision response remains `dun-9qub`.

## Automatic component gravity and buoyancy follow-up (`dun-spbq`)

The existing extraction ticket now covers isolated connected components of all
rigid materials, including stone and wood. Detaching a supported component must
start gravity automatically, with missing pages handled conservatively. Mass
and density must be derived from component material composition, including
volume-weighted density for mixed-material assemblies. Buoyancy follows displaced
water volume, while drag and torque use submerged shape and fluid motion. Stone
and low-density wood therefore sink or float according to their physical density,
without a manually selected sink/float flag. The ticket links the existing stone,
wood, and Eulerian coupling tasks and requires mass and identity preservation
through chunk seams, storage saturation, and saves. That behavior remains pending;
the current contact implementation does not extract components or apply buoyancy.

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

### Implemented GPU contact bounds and streaming behavior

The enabled rigid solver uses eight substeps per tick, each with eight Jacobi
position iterations. Linear velocity components are clamped to four cells per
tick; dynamic bodies receive a downward 0.25-cell/tick velocity increment once
per tick. Angular velocity is bounded to the smaller of 0.5 radians/tick and
two divided by the body's maximum vertex distance from its material-derived
center of mass. Thus predicted vertex displacement per substep is at most
`(sqrt(2) * 4 + 2) / 8 < 1` cell. Contacts are regenerated each substep with a
0.25-cell speculative skin. This is discrete collision detection, not swept
time-of-impact CCD; the displacement bound does not guarantee collision with
arbitrarily thin subcell features or arbitrarily thin moving body pairs.
One-cell terrain hard drops, four-cell body overlap resolution, stable stacks,
and moving supports are covered by the Vulkan regressions.

Terrain edits are sampled on the next rigid tick. Removing the final support
therefore releases a resting body immediately without cached contact impulses
holding it in place. Initial overlap is corrected by the same position solve;
corrections determine the bounded dynamic velocities afterward. An incomplete
contact set rolls back the entire tick instead of applying partial corrections.

A body whose reference page is missing retains its world pose, linear velocity,
angle, and angular velocity. Any part still overlapping resident geometry acts
as stationary support with zero contact velocity and zero inverse mass/inertia.
Every slot records its actual substep starting pose, including frozen slots,
so friction measures displacement from that pose rather than from zero. These
effective contact properties are temporary shader values; reloading the
reference page reactivates the stored dynamic state. Awake kinematic bodies
retain their prescribed contact velocity so moving supports still carry bodies.
All of this runs on GPU; state reads in regression tests are explicit diagnostics.

Concave bodies use two to four authored convex pieces under one stable body ID,
pose, velocity, and angular state. Pieces must share one material, have disjoint
interiors, and form a connected shared-edge graph; invalid updates are rejected
before modifying the body pool. GPU mass, centroid, and inertia sum the piece
integrals with the parallel-axis theorem. Bounds and angular speed limits cover
every piece. Occupancy rasterizes their union and narrowphase tests individual
piece pairs, so a concavity remains empty for both rendering and contacts.
Binding 4 uses 528-byte body records with piece storage at byte 240; broadphase,
contact, and solver sections still follow the 64-record prefix. Snapshot V4
stores 432-byte body records including compound pieces; V1–V3 remain readable.

`make test_rigid_dynamics` runs cavity/mass/persistence regressions and a paced
quarter-native Vulkan window scene. A green stone L and blue wood U fall with
initial angular velocity, collide, and settle. The window uses an eight-times
nearest-neighbor crop of the full 640×448 simulation with 70 resident chunks,
fluid interval two, and 16 pressure sweeps. The scene checks body/body and
terrain contacts, visible rotation, bounded final velocities, and two active
rigid IDs. It writes before/impact/settled BMPs and a 30-frame/s RGBA replay
stream under `build/screenshots/rigid_concave*`. Both `test_ui` and
`test_quarter_native` include this visual scene; `make test` includes the
headless compound regressions.

## End-to-end gates

Each implementation ticket requires a headless state/budget test and a
windowed scene once the behavior is visible. Test sequences include chunk
seams and save/reload, mixed material painting, dry and wet controls, gentle
and hard stone drops, floating and sinking bodies, and accelerated long-time
wear/rot. Validate Vulkan synchronization and record GPU stage timing at the
same stage as the feature; capacity and memory traffic are part of the
implementation, not postponed to the resolution benchmark.
