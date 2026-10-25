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
acoustic speed. Actual calibrated sound/substep cost, buoyancy, geometry changes,
the displayed scene and persistence remain acceptance work. No smoke module is
complete from these CPU results.

The reference equations and independent wave-curve construction are checked
against the primary [Clawpack Euler reference](https://www.clawpack.org/riemann_book/html/Euler.html)
and [approximate-solver reference](https://www.clawpack.org/riemann_book/html/Euler_approximate.html).
An expansion-aware low-Mach alternative would require evolving closed-chamber
pressure, as described in the primary
[PeleLMeX model](https://github.com/Pele-Suite/PeleLMeX/blob/development/Docs/sphinx/manual/Model.rst).
Reusing water's zero-divergence projection would not satisfy that contract.

## Vulkan transport checkpoint

The optional Vulkan backend now uses parallel HLLC transport, compensated energy
and transactional scratch banks. It completes requested time with adaptive CFL
substeps; only reduced scheduling scalars return to the CPU, never a runtime
cell field or component list. Subgroup reduction avoids a global atomic per cell.
An invalid interval leaves authoritative stock and its physical clock unchanged.
The renderer composites conserved pigment from the gas buffer without scanning
lights or advancing simulation, and the disabled path allocates no gas storage.

Two actual Wayland/Vulkan tests pass at 480x270 physics/render with a 640x448
halo, including exact resting/pause invariance and colored transport through a
wall opening against the independent CPU oracle. The closed transport energy
error is 0.0000343876891 and red-pigment error -0.0000000210703321. The displayed
capture is `build/screenshots/gas_reference_transport.bmp`. Two existing thermal
feedback display tests also pass after adding the optional renderer hook. This
is a transport checkpoint, not the interactive or buoyant smoke-box acceptance.

## Buoyancy and calibrated prototype

The reference-atmosphere force uses composition and temperature, with downward
acceleration `g * (1 - R_mix*T / (R_air*T_ambient))`. This is a filtered buoyancy
approximation: acoustic density fluctuations do not create a second gravity
force or an artificial hydrostatic density gradient from the reduced acoustic
speed. Hot air rises, cold heavy exhaust sinks, and canonical ambient gas stays
exactly stationary within floating-point roundoff. The impulse retains species
and pigment; its exact kinetic-energy work is added to total energy and tracked
separately as external work. It is not a combustion energy source.

Nine CPU reference and three actual-display Vulkan cases pass, including the
hot impulse against the independent reference and energy-minus-work closure.
The permanent `build/diagnostics/gas_frame` probe measures events, unchanged
mechanics, gas and actual presentation at 480x270 with all 640x448 cells mapped.
Initial prototype coefficients are R={.25984,.29680,.18892,.46152,0},
Cv={.65816,.74320,.65508,1.40248,.71}, kinetic scale1/512, ambient density.0012,
temperature293.15K and gravity900 cells/s². These are explicit game calibration
parameters, not a claim of SI acoustics. The CFL bound and budget tolerances
remain unchanged. Whole-frame measurements must decide whether this prototype
is viable before the interactive module is accepted.

The first actual-display full-field probe failed:30 measured frames after20
warmup, mean111.907/worst119.253ms (gas109.963ms),1107 gas substeps across50
physical ticks. A stationary-face shortcut reduced this only to98.377/115.135ms.
A shared-face buffer experiment retained all3 displayed numerical cases,
including strengthened moving-wall coverage, but worsened cost to163.711/173.258ms.
The added flux-buffer bandwidth is not viable; this experiment is retained in
git history rather than selected as the production backend. No target, clock,
budget tolerance, velocity or substep limit was changed to pass this gate.
