# Moving rigid boundaries and water volume

The fluid boundary uses the final rigid occupancy raster. At the beginning of a
fluid update, one GPU invocation owns each viewport column and transfers water
from occupied cells upward into resident, permeable capacity. Transfers use
integer 16.16 water units and a fixed traversal order. No two invocations write
the same column, so this displacement is reproducible without atomic allocation
or a CPU solve.

Solid terrain, missing pages and the viewport boundary stop the search. Occupied
cells can be crossed within a column, but cannot receive water. When upward
capacity is unavailable, the unresolved volume stays at its original world cell;
it is retained overflow rather than deleted water. Ordinary fluid transport
cannot move that volume until the occupancy clears or a subsequent displacement
update finds capacity. Lateral redistribution around a blocked column is not
implemented; callers can explicitly inspect the retained overflow with
`dc_gpu_fluid_displacement_overflow`. This diagnostic waits for GPU state and is
not used by the ordinary frame loop.

The authoritative chunk water fields include both mobile water and retained
overflow. Their existing streaming and checkpoint transactions therefore preserve
the total without a separate reservoir or a changed file format. Removing a body
reopens its cells and permits normal water transport on the next fluid update.

Buoyancy, drag and submerged-shape torque remain separate work in `dun-sbk9` and
`dun-08qq`; the displacement pass alone does not exert forces on bodies.
