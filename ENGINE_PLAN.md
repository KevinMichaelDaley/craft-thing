# Dungeoncraft engine plan

## Goal and initial scope

Build a 2D, Noita-style material sandbox in C11. Vulkan runs both simulation and drawing; compute and graphics shaders compile to SPIR-V. The CPU owns the window, input, asset loading, chunk streaming, scheduling, and diagnostics. Material motion and body integration stay on the GPU during normal frames. The first renderer displays colored materials, rigid bodies, and a camera with nearest-neighbor scaling. Every world cell is individually simulated while its chunk is active. At zoom 1, one world cell maps to one display pixel; whether a full native-resolution viewport can run at the target tick rate is a benchmark question.

Initial target: one desktop Vulkan 1.3 device, one graphics/compute queue, and a fixed 60 Hz simulation tick. Benchmark 1024 x 576, 1920 x 1080, and 3840 x 2160 active-cell viewports before choosing a default internal scale or minimum GPU. A headless Vulkan path must run the same simulation for tests and captures. Determinism is required for a fixed device and shader build; cross-vendor bit-identical results are a later goal.

## Simulation representation

- Divide the unbounded world into 64 x 64 cell chunks addressed by signed 64-bit chunk coordinates. The GPU keeps only a bounded pool of resident chunks. A CPU chunk directory maps world coordinates to persisted or procedural data; a GPU page table maps resident coordinates to pool slots. Track active, sleeping, loading, and dirty chunks separately. Neighbor stencils use halo cells or direct neighbor-slot lookup, refreshed at each substep that crosses chunk edges.
- Store solid/powder identity and state in ping-pong integer cell layers within the resident chunk pool. Keep Eulerian fluid volume and velocity/flux in separate ping-pong layers: values live at fixed world cells and fluid moves by conservative flux between cells, never by tracking fluid particles. One cell may contain a solid or powder plus fluid volume only where the material rules permit it. Define that rule in the material table, not in pass-specific code.
- Use a small, bounded pool of **massless virtual markers near water interfaces** to guide free-surface reconstruction and reduce visible smearing. Water volume remains authoritative in Eulerian cells; marker corrections may only request equal-and-opposite cell transfers. The detailed solver and verification plan is in [design/fluid_solver.md](design/fluid_solver.md).
- Store rigid bodies in structured buffers: stable ID, transform, linear/angular velocity, mass/inertia, sleeping flag, and shape reference. Shapes are small masks or convex pieces in a separate buffer. A transient rasterized occupancy image links bodies to world cells without making the cell grid authoritative for body motion.
- Keep material definitions in a GPU table: density, phase, color, mobility, friction, viscosity/flow parameters, and reaction IDs. Use integer or fixed-point fields for cell mass and transfers so concurrent proposals can be resolved predictably. Keep material IDs stable for saves.
- Reserve explicit command/event buffers for brush edits, explosions, body impulses, spawns, and reactions. The CPU uploads commands before the tick; GPU passes consume them without a full-world readback.

## Infinite-canvas streaming

Streaming is part of the first architecture, even if the first demo loads only a few chunks. Maintain a fixed GPU slot budget and a world-to-slot lookup; never allocate a Vulkan image per world chunk. The resident set covers the camera viewport, a safety margin for motion and fluid propagation, and chunks around active bodies/events. Empty untouched chunks can be generated from a world seed on demand. Modified chunks are persisted with material state, fluid state, version, and stable IDs. The CPU uses signed 64-bit world coordinates; shaders operate on resident slot IDs and coordinates relative to a rebased local origin so a device-wide 64-bit integer shader requirement is unnecessary.

A separate CPU worker thread handles chunk generation, disk loads, and saves through bounded request/completion queues. The main thread owns Vulkan staging and submission; it processes completed worker jobs without waiting for disk I/O. A slot becomes visible to simulation only after upload completes. Dirty chunks are copied back asynchronously and saved before their slots are reused; GPU timeline values or fences guard both upload visibility and safe eviction. Pin chunks touched by the current tick and their stencil neighbors. When the resident budget is exhausted, evict clean sleeping chunks first, then saved dirty chunks; expose a budget-pressure counter rather than silently discarding state.

Chunk-edge rules must conserve mass and particles. If a destination chunk is absent, request it and defer transfer until it is resident; do not treat an unloaded edge as empty or delete outgoing material. Rebuild halos after neighbor changes and before each stencil substep. The initial offscreen policy is that sleeping or unloaded chunks preserve their state and do not advance time. A later design decision can add coarse offscreen simulation if continuous world evolution is required.

## Tick graph

The required top-level order is rigid bodies, fluids, then custom falling-sand passes. Each stage sees the completed outputs of the previous stage. The frame graph records `vkCmdPipelineBarrier2` dependencies for every write-to-read or write-to-write transition. Start on one queue to avoid queue ownership and async compute complexity.

1. **Resolve residency / apply commands / wake chunks.** Admit completed uploads, pin the required neighborhood, clear transient counters, apply queued edits, and mark affected chunks plus neighbors active. Never start a tick with a missing required neighbor silently interpreted as empty.
2. **Rigid-body pass.** Integrate body velocities at fixed `dt`; rasterize body shapes into transient occupancy; detect terrain/body contacts; accumulate contact impulses and correct transforms through a bounded solver iteration. Publish final occupancy and swept cells for later passes. The first milestone supports circles/boxes or small bitmap masks and terrain collisions. Body-body collision and fractured terrain follow after the single-body path is stable.
3. **Fluid pass.** Read the final rigid occupancy and terrain permeability. Compute directional, conservative Eulerian fluxes between fixed cells into a separate buffer; resolve source/destination limits and update fluid mass with ping-pong state. Use several local substeps only when the chosen speed/CFL bound requires them, refreshing chunk boundaries between substeps. Treat moving bodies as obstacles that can displace fluid into neighboring capacity; document where exact volume preservation is deferred.
4. **Custom falling-sand passes.** Run ordered rule groups: reactions and phase changes, powders/granular motion, gases, then cleanup/activation. For moves, generate intents from an immutable source grid, choose one winner per destination with deterministic priority, and commit into the destination grid. Rotate lateral preference by tick to avoid one-sided bias. Reactions that touch multiple cells use the same claim/resolve scheme. Swap ping-pong grids only after a group completes.
5. **Finalize.** Update active-chunk and dirty flags from changed cells, compact event output, and expose a consistent snapshot to the renderer. Queue newly sleeping dirty chunks for persistence. Rendering samples only finalized state.

```text
CPU commands -> rigid integrate/contact/occupancy
             -> fluid flux/resolve/update
             -> material reactions -> powder -> gas -> cleanup
             -> simple composite -> swapchain
```

The stage names describe ordering, not necessarily one dispatch each. A Vulkan barrier makes writes visible across dispatches; it does not make unordered writes inside one dispatch safe. Every pass therefore has explicit ownership of its output or a proposal/resolve step.

## Rigid-body / cell coupling

Use the body state as the source of truth for motion. Rasterized occupancy is rebuilt each tick, with stable body IDs in cells for contact lookup. The rigid solver samples nearby cell material and collision flags; the fluid pass sees body occupancy as a boundary; the granular pass cannot move into occupied cells. Terrain edits made by later passes affect rigid collision on the next tick. This one-tick delay makes the requested ordering unambiguous.

When a body sweeps across cells, track both current and previous occupancy so it cannot tunnel through thin terrain solely because its final position is clear. Initially clamp body speed/displacement or use bounded substeps; add continuous collision only if tests expose a need. For body-body pairs, build broad-phase candidates from occupied chunks, sort/unique pairs, then solve contacts without floating-point atomics.

## Vulkan and renderer

- Require Vulkan 1.3, synchronization2, storage-image format support for the chosen integer/float formats, and suitable workgroup limits. Probe features/formats at startup and report a specific unsupported requirement.
- Compile shader sources to SPIR-V at build time and check reflection/layout expectations during development. Keep shader bindings declared in one shared C header plus shader include definitions or generated metadata, with layout assertions on the C side.
- Use storage images or SSBO arrays for pooled cell layers and an SSBO for bodies/material tables. Dispatch over a compact active-slot list; use `vkCmdDispatchIndirect` after the active-list path is verified. Avoid a dispatch sized to the logical world, which is unbounded.
- Render a full-screen triangle that maps screen coordinates through the GPU page table to resident chunk cells, then samples finalized material/fluids and a palette table. Draw rigid body colors from occupancy/ID data. Begin with nearest sampling, a camera offset/zoom, and one debug overlay for occupancy, chunk residency, or fluid mass. No lighting or texture pipeline is needed yet.
- Present via a swapchain with binary acquire/present semaphores. Timeline semaphores can track CPU/GPU frame reuse later; they are not needed for the initial single-queue tick.

## Code layout

```text
include/dungeoncraft/  public C API and shared data layouts
src/core/             app loop, allocators, logging, input
src/vulkan/           device, resources, descriptors, pipelines, barriers
src/sim/              tick graph, chunk management, materials, bodies
src/world/            chunk directory, streaming, persistence, generation
src/render/           swapchain and simple presentation
shaders/sim/          compute shader sources
shaders/render/       full-screen draw shaders
tests/                headless GPU behavior and conservation tests
tools/                shader build and capture helpers
```

Use a Makefile with C11 and `-Wall -Wextra`; keep runtime dependencies limited to a window library and the Vulkan loader. Shader compiler and validation layers are development dependencies. The public API should initially expose create/destroy, load/save, queue command, tick, render, and readback/capture operations.

## Delivery sequence and acceptance checks

1. **Vulkan shell:** window, validation-clean device/swapchain, SPIR-V build, headless compute dispatch, colored grid display. Check: one shader writes a grid and the renderer shows it.
2. **Chunk foundation and cell rules:** bounded resident pool, coordinate-to-slot lookup, material table, double-buffered cells, active-slot bookkeeping, and sand movement with conflict resolution. Check: stable pile, no duplicated/lost particles across chunk edges, fixed-device replay hash.
3. **Rigid coupling:** body buffer, occupancy raster, terrain collision, body visible in renderer. Check: body rests on terrain, does not enter solid cells, and responds after terrain removal on the next tick.
4. **Fluids:** conservative flux/update with body and terrain boundaries. Check: basin mass is conserved within defined fixed-point tolerance, fluid cannot cross impermeable cells, moving body displacement behavior is recorded.
5. **Streaming and full tick:** enforce rigid -> fluid -> falling-sand order, add reaction rules, async chunk load/save/eviction, and debug overlays. Check: camera crosses many chunk boundaries; dirty sand/fluid state survives eviction and reload; mixed scene replay and stage-by-stage capture show no Vulkan validation errors.
6. **Resolution benchmark and scale work:** measure 1024 x 576, 1920 x 1080, and 3840 x 2160 active viewports with rigid/fluid/sand scenes. Record GPU pass times, CPU submit/streaming time, peak resident memory, upload/readback bandwidth, and frame pacing. Test cold camera movement and dense active scenes. Decide native-resolution support from measured 60 Hz results; then optimize active-slot dispatch, memory traffic, and synchronization against bottlenecks.

The first playable demonstration is a small map where a rigid box falls into sand beside a fluid basin, a brush adds/removes cells, and the user can pause and advance one simulation tick. Save a stage-by-stage capture and a state hash for regression diagnosis.

## Design decisions to settle before implementation expands

- Whether the Eulerian fluid solver should use a simple volume/flux model or add pressure/velocity projection for more detailed flow. Both keep fluid state at fixed world cells; the latter costs more passes and memory bandwidth.
- Whether rigid bodies may consist of destructible material cells. The first implementation treats them as persistent shapes; breakage/fracture can be a later conversion from body pixels into free material cells.
- Whether unloaded chunks should continue evolving offscreen. The initial policy pauses their simulation and preserves exact saved state; continuous evolution requires an explicit bounded approximation or background simulation budget.
- Target platforms and minimum GPU. The Vulkan 1.3 baseline currently assumes desktop hardware; mobile and older GPUs would require a feature and format fallback plan.

Vulkan API references: [compute shader dispatch and indirect dispatch](https://docs.vulkan.org/guide/latest/compute_shaders.html), [compute write/read synchronization examples](https://docs.vulkan.org/guide/latest/synchronization_examples.html), and [storage image usage](https://docs.vulkan.org/guide/latest/storage_image_and_texel_buffers.html).
