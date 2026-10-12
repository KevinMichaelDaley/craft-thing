# Moving rigid boundaries and water volume

The final GPU rigid raster retains stable occupancy IDs and swept IDs, plus three
16.16 planes for exact solid cell area and covered horizontal/vertical faces.
Convex pieces share polygon clipping with submerged-volume integration. Fluid
receiving capacity is the cell area left by the solid, and pressure/transport use
the open face aperture. A partial cell cannot pass water through a covered face.

At the beginning of a fluid update, one invocation owns each viewport row and
fills nearby resident capacity with displaced water, searching at most 64 cells
to either side. A barrier precedes the column pass, which lifts remaining excess
into upward capacity. This fills vacancies left by sideways motion before
raising water into the free surface. Integer transfers and fixed traversal order
make both passes reproducible without atomic allocation or a CPU solve. Solid
terrain, missing pages and viewport boundaries stop each search.

Unplaceable volume remains at its original world cell as retained overflow.
`dc_gpu_fluid_displacement_overflow` explicitly reports water exceeding the cell's
available area; ordinary frames do not call this diagnostic readback. Chunk
water fields preserve mobile water and overflow through streaming and checkpoints.
No extra reservoir or file format is required. Removing a body reopens capacity
and faces for subsequent fluid updates.

# Buoyancy and viscous drag

The GPU solver clips each rigid piece against immersed cell volume, integrates
water gravity at the displaced-volume centroid, and applies velocity-relative
drag and torque. Mass/inertia derive from material density; water density is one,
ordinary wood is 0.6, dense wood 1.2, mortar 1.6 and stone 2.7. No body-specific
sink/float flag or density override exists. Dry bodies skip hydrostatic work.

For terrain-bounded pools, up to 16 clear columns on either side reconstruct the
ambient hydrostatic surface from their water volume. Solid/cold barriers stop
sampling. This excludes temporary films and wake gaps produced by geometric
rezoning. Unbounded/open-flow cases use local wet-cell sampling. The projection
initializer carries hydrostatic head through submerged rigid regions so a static
immersed body does not generate artificial circulation.

Viscous grid-volume relaxation has rate 0.03 per tick. With the minimum material
density 0.6, full immersion damps relative motion by at most one twentieth per
tick; partial immersion is weaker. Anchored structural sources transfer the same
hydrodynamic loads to their force/moment graph. Fluid changes affect the next
rigid tick without ordinary-frame copyback. Long floating tests check mean draft
and bounded waves, and sinking tests check terrain settling and conserved mass.
