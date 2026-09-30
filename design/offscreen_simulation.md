# Distance-based offscreen simulation

## Current implementation

Dynamic chunks beyond the camera view live in a bounded background GPU cache.
Each cache grid covers up to 3×3 chunks and retains their material, Eulerian
mass and velocity, markers, and MPM grains on the GPU. The runtime can hold at
most 16 grids; empty terrain needs no cache slot. Nearby grids advance every
4 ticks, middle grids every 12, and farther grids every 24. Multiple physics
substeps with the same timestep share one GPU submission, including the barriers
between steps. A grid beyond four screen widths is saved through the worker and
released. Readback and upload occur at chunk eviction, promotion, and save,
not on ordinary frames.

The cache prefetches saved dynamic chunks as the camera approaches. Water,
grain identity, velocity, and conservative transfers across adjacent grids
have camera-pan regressions. Under a saturated multi-directional load, the
cache can still exhaust its 16-grid capacity; recycling and fairness remain
tracked by `dun-5a1a`. Due grids record GPU ticks into separate command
buffers and submit them together. Water-only grids skip MPM entirely; gas and
grain activation propagates when a GPU boundary exchange transfers either
material into another grid. Near, middle, and far grids use 8, 6, and 4
pressure sweeps respectively.

## Problem and invariant

The current page table is exactly the rendered view plus one 64-cell chunk on
each edge. A chunk outside that rectangle is saved and evicted, so a camera pan
can reveal water and grains that stopped at the old boundary. Enlarging the
rectangle to several full screens is not viable: per-slot particles, markers,
and cell state grow with the area even when the surrounding terrain is idle.

World chunks, rather than screen pixels, own material, fluid volume, particles,
markers, face velocity, and their last completed simulation time. The viewport
is only one consumer of these chunks. A chunk promoted into the visible set
must be caught up to the current world time before it can be rendered. Loading
or saving a chunk must preserve its pending boundary flux and simulation time.

## Work and residency

Keep a compact GPU cache for active offscreen chunks and use a separate bounded
simulation workspace for a chunk tile and one-chunk guard. The worker thread
continues to generate and save chunks. Main-thread Vulkan commands upload or
download only when a tile enters or leaves this cache, never as part of every
presented frame. Idle terrain needs no GPU slot or periodic solve. GPU state
inside cached chunks remains device-resident between updates; face velocities
must be stored by chunk, rather than only by viewport-grid position.

The scheduler classifies a chunk by its distance from the **edge** of the
visible rectangle, normalized separately by view width and height. A chunk
within one screen of either edge is near, one to two screens is middle, and
two to four screens is far. Beyond four screens, it sleeps until it approaches
again. The classification uses the larger normalized axis distance so a
diagonal chunk does not receive an unjustifiably high update rate.

| Band | Distance beyond view | Target update period | Pressure work |
| --- | --- | --- | --- |
| Visible and seam guard | 0 | every tick | full |
| Near | up to 1 screen | 4 ticks | reduced, residual checked |
| Middle | 1–2 screens | 12 ticks | reduced, residual checked |
| Far | 2–4 screens | 24 ticks | minimum stable work |
| Dormant | more than 4 screens | none | none |

The periods are scheduling targets, not slower simulated time. Every chunk
advances on the same world clock. The elapsed interval is divided into GPU
substeps of at most 1/20 second for the tested free-surface limit. Conservative
fluid transport traverses the velocity-times-timestep distance, including
multiple cells at high speed, and stops at real cell boundaries. A regression
starts water at eight cells per tick in the far band and verifies downward
travel, exact volume, and no high spray. Any pending fraction is carried to
the next update; promotion to the visible band drains that debt first.

## Band interfaces

Neighboring chunks can have different due times. A conservative face flux
belongs to both chunks: removing mass from one and adding it to the other must
be one GPU operation or an equal-and-opposite queued ledger entry. The same
rule applies to MPM particles crossing a boundary. A sleeping or unloaded
chunk is not silently treated as a permanent wall; unresolved flux remains
queued for that world face. Chunk eviction flushes the ledger and velocities
to durable state. When the camera moves, newly visible chunks and their seam
guards are promoted together, then their accumulated flux is applied before
rendering or accepting input.

The GPU seam pass joins the projected interior face momentum from both
workspaces before applying the head and pressure difference at the shared
face. This preserves horizontal advection for deep water when the source and
destination seam cells are both full; relying on a local head difference alone
left part of that volume against a false boundary. The same symmetric
calculation handles reverse and vertical flow. Flux still removes exactly the
volume added on the other side and reaches only cells allowed by face velocity
times elapsed world time. Split-versus-monolithic deep-basin tests compare
the destination volume after 120 ticks, while the UI smoke sends a large water
patch past the visible/offscreen seam and checks that it travels eight cells
into the destination chunk.

Offscreen workspaces retain 92% of face velocity per 1/60-second world tick,
compared with 99.7% in the visible workspace. The GPU fluid correction raises
that factor to the elapsed-tick power, so a low-cadence offscreen update damps
the same amount of momentum as its equivalent sequence of short ticks. This
attenuates edge oscillation without changing cell mass or adding a readback.
The GPU regression compares equal-time visible and offscreen steps, verifies
faster offscreen velocity decay, and checks exact water mass.

A synthetic saturated-ring benchmark (`make bench_offscreen`) allocates 16
active 3×3 workspaces sharing a 1920×1080 parent GPU. With one deep-water chunk
per workspace, near-band work fell from about 15 ms to 9–10 ms per world tick
after idle MPM skipping and batched submission; middle and far work measured
about 4–5 ms per tick. The foreground and offscreen budgets are additive, so
native-resolution throughput remains tracked by `dun-vd9a`.

## Validation

An end-to-end camera test paints water in a chunk, pans a screen away, runs
60 world ticks, returns, and checks that the water fell while offscreen. Repeat
at two screen widths and with grains, including chunk seams. Compare total
water and particle IDs before and after demotion/promotion. A long pan should
show no frozen strip, missing mass, or velocity reset. Profile GPU stage time,
streaming copy count, and worst frame time with a saturated offscreen ring at
half-native and native resolution; the offscreen cache must remain bounded.
