# Distance-based offscreen simulation

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

The periods are scheduling targets, not larger unconditional fluid steps. Every
chunk advances on the same world clock. The elapsed interval is divided into
GPU substeps that respect the maximum face displacement and the tested
free-surface step limit. Pressure iterations can fall with distance only while
the projected divergence remains bounded. If a flow needs more substeps or
iterations, the scheduler budgets them over following frames and never drops
elapsed world time. Promotion to the visible band drains that debt before the
chunk is shown.

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

## Validation

An end-to-end camera test paints water in a chunk, pans a screen away, runs
60 world ticks, returns, and checks that the water fell while offscreen. Repeat
at two screen widths and with grains, including chunk seams. Compare total
water and particle IDs before and after demotion/promotion. A long pan should
show no frozen strip, missing mass, or velocity reset. Profile GPU stage time,
streaming copy count, and worst frame time with a saturated offscreen ring at
half-native and native resolution; the offscreen cache must remain bounded.
