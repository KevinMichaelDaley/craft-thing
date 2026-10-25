# Smoke and ventilation box

Active visible milestone: `ctfs-dmxv`, after delivered heat bench `ctfs-2hu1`.
One conserved gas field will support finite colored injections, a real vent,
sideways/buoyant flow and oxygen feedback. Combustion and submerged Greek-fire
coupling remain in the broad gas/render tickets. No runtime CPU field solver,
component list, per-flame light scan or global gas/fire quota is introduced.

## Numerical reference gate

Gas state holds five species (oxygen, nitrogen, exhaust, vapor and soot), total
energy, momentum and RGB weighted by admitted exhaust/soot mass. Pigment is not
another mass inventory. Oxygen feedback uses molecular fraction; inert injection
dilutes it while preserving oxygen stock. Solid surfaces close flux and fixed
walls perform no boundary work. Opening a vent creates empty available volume,
rather than an unexplained fresh-air source.

The isolated double-precision CPU oracle implements mixture Euler thermodynamics,
conservative HLLE and contact-preserving HLLC with a positivity fallback. Runtime
transport will remain on Vulkan. Pressure includes an explicit conversion from
grid kinetic energy to material heat units. Reversible compression accounts for
boundary work, retaining all species and color; an invalid step never publishes
partial output. The multidimensional explicit CFL limit is 0.4, evaluated from
the acoustic and advective characteristic speeds. The requested physical interval
must be completed using as many admitted substeps as necessary, without a fixed
velocity/substep cap or discarded time.

Eight reference tests pass with zero compiler warnings. They cover sealed
heating, reversible compression/expansion, finite packet mixing, stationary
colored contacts, both fluxes' closed inventory budgets, CFL rollback/pause,
two-dimensional solid walls and a vent, and an independently derived exact Sod
solution. The 2D vent transfers color only after opening and retains species,
energy and pigment. Exact-wave mean absolute errors at 128/256 cells:

| Flux | Cells | Density | Pressure | Velocity |
|---|---:|---:|---:|---:|
| HLLE | 128 | 0.014768637 | 0.012664552 | 0.025322001 |
| HLLE | 256 | 0.009585720 | 0.007710365 | 0.015288547 |
| HLLC | 128 | 0.013947681 | 0.012324991 | 0.024495569 |
| HLLC | 256 | 0.009051956 | 0.007539482 | 0.014927237 |

All meet the predeclared 0.025 density/pressure and 0.05 velocity bounds, and
both refine toward the independent solution. HLLC is the initial GPU transport
choice because it preserves stationary contacts instead of mixing colors at the
acoustic speed. Actual calibrated sound/substep cost, buoyancy, geometry changes,+the displayed scene and persistence remain acceptance work. No smoke module is
complete from these CPU results.

The reference equations and independent wave-curve construction are checked
against the primary [Clawpack Euler reference](https://www.clawpack.org/riemann_book/html/Euler.html)
and [approximate-solver reference](https://www.clawpack.org/riemann_book/html/Euler_approximate.html).
An expansion-aware low-Mach alternative would require evolving closed-chamber
pressure, as described in the primary
[PeleLMeX model](https://github.com/Pele-Suite/PeleLMeX/blob/development/Docs/sphinx/manual/Model.rst).
Reusing water's zero-divergence projection would not satisfy that contract.
