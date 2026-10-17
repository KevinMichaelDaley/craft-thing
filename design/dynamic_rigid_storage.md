# Dynamic rigid storage

Epic ct-hhmy implements the October 17 user requirement: body count grows with the
simulation and is not bounded by a compiled maximum. Raising 64 to another number
does not satisfy this design. The user's saved material sandbox contains 64 active
owned bodies and only 2,722 geometry words; this exhausted body records and left
later isolated stone as gray terrain. Density changes cannot fix that condition.

This is a proposed implementation roadmap, not a claim that growth is implemented.
ct-yd8t separately tracks extraction diagnostics and known offscreen dynamics.

## Storage and identity

Separate body records from broadphase/structural data. The existing binding-4
`Body bodies[64]; uint data[]` layout makes every later table depend on the maximum
body count. Replace it with a runtime-addressed body store and independently owned
tables. Keep the public body's physical fields, exact canonical pixels and stable
runtime ID semantics. A slot handle includes a generation; freeing or relocating a
slot must not let queued contacts or descendants target a different body.

Use growable slabs and a growable directory, with record, motion/sleep state,
ownership/provenance and scratch resources budgeted separately. A slab size is an
allocation granularity, never the maximum number of slabs. The preferred Vulkan
implementation uses buffer device addresses and GPU-addressed directory pages so
one descriptor range or one storage-buffer-size limit does not become another
body-count constant. Audit/enable the required device features explicitly and
validate alignment/address bounds before selecting this ABI. Report actual device
or allocation failure; do not silently choose a fixed-capacity physics fallback.
The address contract should use `uvec2` and `GL_EXT_buffer_reference_uvec2`, avoiding
an unnecessary `shaderInt64` requirement; see the
[Khronos BDA guide](https://docs.vulkan.org/guide/latest/buffer_device_address.html)
and [GLSL extension](https://docs.vulkan.org/glslext/latest/glslext/ext/GLSL_EXT_buffer_reference_uvec2.html).

Structural graph node/member limits describe authored graph topology only. They
must not bound automatic bodies, descendants or the global body allocator.

## Allocation and simulation boundaries

GPU passes compute live/free masks, hierarchical prefixes, deterministic ID and
slot reservations, and required record/geometry/work storage. Birth and fracture
jobs remain pending until every dependent reservation succeeds. Publication is a
single transactional state change after parallel copies, properties and validation;
source material and persisted serials remain unchanged on any failure.

Vulkan memory allocation, descriptor updates and object destruction necessarily
run on the host. A small completed allocation-request mailbox communicates required
bytes/counts at a simulation boundary. This is resource coordination, not a CPU
component solver or geometry/physics readback fallback. Use completed fences and
deferred old-resource destruction; never rewrite descriptors still used by an
in-flight submission. Copy/relocate payloads and patch GPU directories in parallel.
Do not poll or copy every body on the host to advance ordinary frames.

The allocator prototype separates five specialized passes. A 64-lane group per
slab counts reusable free slots and owned slots, excluding retired or exhausted
generations. Hierarchical 128-lane pair scans compute deterministic global ranks;
the host records only the logarithmic dispatch topology. A single invocation
then writes constant-size capacity/status fields. Parallel slab groups write
tentative generation-bearing handles, followed by parallel owner/mask commits
only when the joint transaction's commit gate is set. A final single invocation
publishes only the batch count and live count after all owner writes complete.
Neither scalar control invocation scans bodies, requests, pixels or components.
Keeping scalar publication in its own dispatch prevents other workgroups from
observing a changed commit flag halfway through their slot work.

This is not yet the ordinary-frame allocator. Its current bounded fixture records
direct scan dispatches; production integration must build request-dependent
indirect counts so an empty request queue performs zero page/list scan work.
Release/reuse must retain slot generations, reject stale and duplicate handles,
retire exhausted generations, rebuild actual owned/live lists and publish stable
ID lookup metadata in parallel. The current source implements/prepares these
stages incrementally; ticket status and GPU checks distinguish verified pieces
from unverified or missing behavior.

Actual allocation failure produces explicit backpressure and leaves the complete
transaction pending. Removal/reuse, returned memory or a later successful growth
retries deterministically without skipped IDs or duplicated source pixels. Tests
inject allocation failure independently of available machine RAM.

## Runtime work

Use live-body lists and indirect actual-count dispatches. No shader iterates a
compiled maximum to find IDs, body pairs or free slots. Rebuild ID maps and live
lists with parallel scans/hashes, and spatially partition broadphase candidates.
Contacts/manifolds, solver state, hydro partials, raster body lists, graph ownership
and fracture output queues grow from their own measured demand; growing records
alone must not silently truncate another resource. Contact overflow remains
conservative and observable until its complete work can be retried.

The shared accessor contract is `body_storage_layout.h` plus
`shaders/sim/rigid_storage.glsl`. Its prototype has a 64-byte versioned root and
48-byte directory entries pointing to record, solver, ownership and auxiliary
resources. `DC_BODY_AT` resolves a validated slot as an lvalue in an addressed slab;
`DC_BODY_DATA` accesses independently addressed metadata. Renderer consumers must
include this contract instead of reproducing offsets. The current prototype is
used only by bounded ABI tests; ordinary shaders still use the legacy store.

The old slot sentinel equals 64. It must become `DC_BODY_SLOT_NONE` before slot 64
is valid, otherwise an allocation-only patch would corrupt contact/body lookup.
The initial 64-record slab quantum is not a cap on the number of directory entries.

### Migration audit

| Subsystem | Legacy coupling to remove | Required replacement |
| --- | --- | --- |
| Public/internal C state | `body_ids[64]`, host body count, records preceding every metadata table | Counted diagnostic interfaces and GPU ID/live maps; separate resource ownership |
| Buffer sizing/descriptors | `dc_gpu_body_storage_bytes`, body-prefix and scratch offsets | Versioned root plus independently growable addressed tables; fence-safe retirement |
| Shader body access | `Body bodies[BODY_CAPACITY]`, `BODY_STORAGE_WORDS=64*136` | Shared validated slab accessor; geometry handles independent of body-prefix bytes |
| Broadphase | Fixed body masks, slot arrays and capacity dispatch | Sparse spatial lists and actual live-body/page work queues |
| Contacts/solver | Capacity sentinel, per-body scratch and fixed manifold storage | Explicit invalid handles, dynamic state/manifold demand and conservative overflow retry |
| Hydro | Fixed slot lists/64-body reductions/partial offsets | Hierarchical live wet lists and deterministic actual-count reductions |
| Raster/draw | Full capacity scans and fixed tile/body lists | Parallel actual intersection lists; renderer uses the same storage helpers |
| Extraction/fracture | Free-slot scan, output arrays, ID search and source/arena transaction | Parallel dynamic allocator, output preflight and joint publication |
| Structural ownership | Per-body records sharing node/member allocation assumptions | Dynamic body/descendant handles independent of authored graph capacities |
| Checkpoints | Fixed 64-record payload and absolute legacy geometry pointers | Counted canonical records/geometry, migration and all-allocation preflight |
| Tests/tooling | Private header/SPIR-V pairs and minimal fixture features | Explicit storage ABI guard, BDA feature/flag checks, shared layout and lifetime tests |

The October 17 source audit found 161 named capacity references on 157 lines in
36 files (`build/diagnostics/body-capacity-audit.txt`). Literal constants need a
separate audit: chunk sides and workgroup widths also equal 64 and must remain
unchanged. The following are body-count dependencies even without the capacity
symbol:

- `component_extract.comp` phases 16/17/18/21 use 64 body lanes for arena
  sizing, relocation, free-slot reservation and serial-ID collision checks.
  Candidate selection caps a fair batch at 64; batching can stay bounded, but it
  cannot bound total bodies or make later candidates permanently ineligible.
- `pixel_hydro.comp` subtracts `64*32` when locating solver state. Its planning
  prefixes and force-group binary search cover exactly 64 bodies. Shared arrays
  of 64 lanes and loops stepping by 64 are team-local reductions and can stay;
  the body-list scan and its upper bound must become hierarchical actual-count
  planning.
- `broadphase_gpu.c` asserts a 64-body shader contract. Its mask-based candidate
  representation cannot be fixed by changing that assertion or widening the
  mask once; use dynamic spatial intersection lists and conservative demand.
- `component_gpu.c` diagnostic selected-count clamping and dispatch-group-to-phase
  inference contain literal 64. Phase selection must be explicit when dynamic
  group counts can equal values that previously identified a different pass.
- `rigid_fragment.glsl` and `pixel_fragment.glsl` must reserve children through
  the same allocator as extraction. Their capacity sentinel, whole-store ID
  search and arena-prefix assumptions cannot survive as a second private pool.
- `structural_store_gpu.c`, `structural_store_validate.c`,
  `structural_store_internal.h` and `body_store_gpu.c` encode the record prefix in
  snapshot words and validation. They must migrate together with host upload,
  owned-source validation and counted snapshot preflight.

The resource-owner prototype allocates one contiguous block for all newly added
slabs, with separate record/solver/ownership/auxiliary planes. CPU loops initialize
only new directory address descriptors; no body payload, geometry, mass or
component is scanned or solved there. GPU transfer commands initialize the block
and copy the old directory prefix. The host may adopt the new directory and
retire its predecessor only after the recorded submission completes. These are
explicit resource-boundary dependencies, not a claim that runtime allocator or
physics consumers have already migrated.

Render visibility is separate from physics residency. Known cached world chunks
can supply extraction and dynamics outside mapped display pages. World-coordinate
terrain/fluid queries and occupied-cell residency checks must use authoritative
registry data, not turn an unmapped viewport edge into a wall. Genuinely missing
connectivity remains UNKNOWN and requests/defer-loads the needed world data.

## Persistence

Introduce a versioned counted body/ownership/slab-independent checkpoint; serialize
canonical records and geometry, not device addresses or transient slab indices.
Read existing 64-record checkpoints, preserving every live body, source ownership,
generation, mass and geometry. Preflight all restored allocations and validation
before replacing the current world. Old snapshot loading must not truncate bodies
when the destination starts with smaller capacity.

## Stages and acceptance

1. ct-q6fr: complete the cross-subsystem ABI/resource audit and feature contract.
2. ct-f83a: implement parallel allocator, growable storage and actual-count queues.
3. ct-kz0x: migrate every runtime consumer; retain exact geometry and physics.
4. ct-6u7r: publish extraction/fracture bursts transactionally through growth.
5. ct-p5x0: migrate checkpoints and ownership safely.
6. ct-ex67: end-to-end and scaling acceptance, including saved-world replay.

RED cases exceed 64 through the actual material-painting level, repeat growth,
mix body types/materials, cross cached seams, remove/reuse records, burst fracture
and births, inject allocation failure, and save/reopen across capacity changes.
Compare camera-independent world dynamics, exact pixels/materials, mass/COM/inertia,
water conservation and stable IDs. Measure growth cost separately from unchanged
frames and preserve existing 480×270 gates and native 640×360 presented workloads.
No milestone closes merely because a larger fixed configuration runs its tests.
