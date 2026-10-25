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

The next gate separates explicit conservative advection from an implicit
acoustic pressure/enthalpy step. Its cold oracle retains variable composition,
finite pressure and expansion rather than imposing zero divergence. A sealed
linear acoustic mode at a timestep far beyond explicit acoustic CFL matches an
independently derived backward-Euler amplitude (0.0149118946719 versus
0.0149118946537), with energy error -1.4552e-11. Stationary colored contacts and
2D pressure release/walls also pass strict stock, energy and pigment checks.
The initial derivation follows the pressure/energy coupling in
[Cordier, Degond and Kumbaro](https://arxiv.org/abs/1108.2876); the implementation
is a separate experimental numerical policy until its real Vulkan reference
and complete-frame measurements pass. Existing HLLC tests remain intact.

The GPU implicit checkpoint passes4 actual Wayland cases, including the original
HLLC cases and the new independent pressure comparison. Core validation passed
before the precision refinement; the refined path has also passed4 displayed
numerical cases. It uses explicit conservative advection, a red/black GPU pressure
solve with reduced residual feedback, and conservative enthalpy flux. All requested
physical time is completed; no acoustic speed or iteration-count cap is imposed.
Stagnation rejects the staged step and retries a smaller interval rather than
publishing partial state. Compensated pressure and product residuals remove a
3–5e-6 float convergence floor without loosening the1e-7 residual target.

An initial compensated full-field diagnostic still failed at193.265ms for its
single measured frame. The next optimization retains the same stencil/tolerance,
skipping a row's expensive update only when a conservative floating-point error
bound proves the row already converged. Rows are rechecked every sweep and wake
when neighbors change. Full-field performance remains RED. Vacuum admission,
geometry edits, streaming/checkpoint ownership and the interactive box remain
unfinished; implicit policy is not enabled in ordinary gameplay.

The precomputed-stencil/converged-row full-field result was129.949ms mean and
184.158ms worst over30 frames. GPU timestamps identify repeated pressure sweeps
as the dominant cost; a targeted Intel compiler probe reports382 instructions,
zero spills/fills for the global pressure shader. A shared-memory block-Jacobi
experiment retained all4 numerical display tests but worsened convergence and
frame cost to360.387/652.893ms (109 accepted substeps over50 physical ticks).
This failed variant is preserved in history. The next experiment uses alternating
tile colors so adjacent tiles consume newly updated boundary data; all acceptance
tolerances and full-frame targets remain unchanged.

Alternating in-place tile colors also retained4 numerical display passes but
still failed at225.300/293.848ms over30 frames. This variant is also preserved
as an unsuccessful experiment. The next optimization uses iterative refinement:
cheap inner float correction solves, compensated outer pressure accumulation,
and the same accurate global residual criterion after each correction batch.

The strengthened native battery now has5 passing cases: both policies retain
exact ambient state, and implicit colored moving-gas transport matches the cold
reference with closed energy error -0.0000449125655 and red error0.000000281586718.
This coverage caught and corrected the GPU's provisional-versus-explicit kinetic
split; the reference/tolerances were preserved, and buoyancy's separately admitted
work remains excluded from spurious internal heating. Iterative refinement reduced
full-field timing to83.998/101.533ms. Compact scalar correction storage and cached
relaxation coefficients reduced it further to58.875/73.470ms over30 frames. The
target remains unmet; next is GPU-discovered pressure work with full residual
checks, preserving every gas cell and avoiding CPU field/list readback.

## Sparse pressure-work invariant

For each staged physical substep, the pressure matrix and source remain fixed
through its correction iterations. The GPU classifies retained row defects using
the same normalized1e-7 criterion as the convergence check. It constructs a solve
list with two tiles of neighborhood and a residual list with one additional tile.
Each list has storage for every grid tile; each tile appends at most once. There
is no truncation, fixed activity quota or CPU list construction/readback.

Inner correction sweeps and compensated accumulation modify pressure only in
the solve list. The matrix has four-neighbor support. Consequently, any row whose
defect can change is inside the extra residual neighborhood and is recomputed.
Outside it, the pressure row and all four neighbors are unchanged, so the retained
defect is unchanged and already within tolerance. The global convergence criterion
therefore includes both recomputed defects and proven unchanged defects. All gas
stock remains stored; this work scheduling does not erase gas, pigment or energy.
Every iteration rebuilds discovery, so changed defects wake neighboring work.

The first indirect-sweep timing was46.552/76.110ms; hoisted advection43.102/83.599ms;
sparse correction/residual coverage40.809/58.540ms over30 frames. These remain
failed performance results, not acceptance. Cross-chunk coverage additionally
compares an8192-cell two-chunk case against the cold oracle and checks a colored
packet crossing the seam, preserving closed species/energy/pigment budgets.

All6 native cases now pass with core and synchronization validation enabled,
including the two-chunk oracle comparison: red0.0332255736 and blue0.0667648464
coexist immediately across the seam, closed energy error -0.00000950787216,
red-pigment error0.0000000998786254. Zero VUID/synchronization errors were logged.
Per-cell advection and acoustic metadata are now computed together; the pending
full-field measurement checks whether avoiding that extra read pass is enough.
This remains a numerical/optimization checkpoint, with the smoke-box interaction,
ownership/persistence and final repeated1000-frame gate still outstanding.

The controllable valve is a gas-face boundary condition, not a smoke animation or
permanent emitter. Parallel rectangle commands open/close arbitrary right/lower
faces without changing volume or stock. Closed faces supply the appropriate wall
reaction independently to each chamber; opening restores conservative transport.
Both HLLC and implicit policies retain bit-exact closed chamber state, then move
finite colored gas when opened. The native battery now passes8 cases. The scene
will use precharged hot colored reservoirs and a controlled enclosure vent; this
fixture valve does not yet claim a constructed mechanical-door implementation.

The real elapsed-clock CLI probe exposed a new force-work bug, not a baseline
failure: a single wall-blocked hot cell gained81.0000381 energy units and normal
momentum -9.00000191 in one0.02s step. Implicit pressure now uses the wall's force
reaction, and external work uses pressure-constrained velocity; rejected provisional
kick energy is removed separately through compensated accumulation. The locked
cell is now bit-exact, with zero energy change/momentum/work. All11 native cases
pass, and the30-tick enclosure still rises and passes its stock/work budgets.

The elapsed-clock closed scene is still RED:519.603/623.152ms before that fix,
349.703/386.935ms afterward. The permanent `smoke_box_frame` stage probe shows
~29ms first-tick rigid preparation, then~1.5ms rigid work. Empty water phases
cost~3.7ms at1/60s and~40ms at0.25s because the generic initial lake/spring latches
fluid work before being replaced by the dry fixture. Gas catch-up also remains
costly. The next cold-authoring optimization avoids generating unrelated water
and prepares static metadata before starting the physical clock. It does not
disable water painting, alter substeps or change the clock.

The dry-scene factory and implicit thermodynamic cache are committed at
f3e00bf. All11 native Vulkan tests and the actual placement enclosure test pass.
GPU activity compaction now stages strict nonzero velocity, force or open-face
pressure differences with an advection halo. Pressure's accumulated residual
coverage adds final affected tiles. Quiet cells remain copied exactly between
transactional banks; the field remains full resolution and every stock survives.
There is no activity threshold, CPU work-list construction or capacity truncation.
The existing eleven native reference/conservation tests pass this sparse path.
The short actual-display probe remains RED: steady1/60 frames33--48ms and
0.25s catch-up265--420ms. These short diagnostics are not final benchmark passes.

The experimental aggregate multigrid preconditioner passes all12 native cases
but is rejected on actual scene cost: approximately4ms per pressure cycle and
1718.912ms for the last0.25s catch-up frame. Coarse aggregates mix disconnected
regions across thin walls and reduce this fixture's residual too slowly. It does
not change the physical operator or tolerance, but is not a production speedup.
Its source is archived in git history; the runtime keeps the validated SOR path.

Grouping four compensated refinement batches in one GPU submission passes the
twelve native cases. Once converged, classification emits empty work lists; the
additional batches do not alter pressure. The same residual tolerance decides
acceptance. A30-frame real elapsed-clock CLI diagnostic improved to41.889ms mean,
73.953ms worst, so the complete-frame performance requirement remains RED.

The implicit advective policy now has a separately validated positivity bound.
For one axis, each Rusanov outgoing coefficient is
`dt * area * (s +/- u) / (2 * own_volume)`, with `s <= global_max_abs_velocity`.
Both coefficients are nonnegative, and each face area is at most own volume.
Their sum is therefore at most `dt * global_max_abs_velocity`; summing axes gives
`dt * (max_abs_vx + max_abs_vy)`. A0.9 safety factor leaves a positive own-cell
weight. Two GPU scalar maxima supply this bound without field readback. This
replaces the implicit policy's overly conservative acoustic-era0.4 coefficient;
explicit HLLC retains its original bound. Invalid thermodynamic candidates still
halve and retry without publishing state or losing time. The cold reference has
a new locked diagonal-flow positivity/budget case and transactional rejection
outside this bound (RED6384ec2); it now passes4/4. Physical clock, source fixtures,
pressure operator and compensated residual tolerance are unchanged.

Scalar32-sweep residual batches pass13 native cases and improve the actual
30-frame elapsed-clock probe to31.762/50.584ms mean/worst. The cheaper shared
block-correction experiment also passed13 numerical cases, but its local barriers
and slower enclosure convergence cost840.708/996.273ms in the actual CLI scene.
It is rejected and archived in history; it is not a production option.

The64-sweep scalar path passes13 native cases and measures26.663/51.330ms on
the30-frame real elapsed-clock probe: average passes, worst frame still fails.
A parallel cyclic-reduction/Peaceman--Rachford ADI experiment also passed13
numerical cases, but cost838.521/963.679ms in that scene. It is rejected; the
shared-memory line barriers are too expensive on this machine. The method's
background is [Peaceman and Rachford](https://epubs.siam.org/doi/10.1137/0103003).
All experimental source is retained in history, not enabled in production.

Lazy row setup checkpoint4d5510e passes13 native cases and the displayed enclosure
test, with a24.243/42.891ms short elapsed-clock result (still RED). Only the exact
advection footprint gets a fresh RHS. Before each pressure batch, its residual
halo gets current diagonal/normalization/relaxation data from cached source
primitives and unchanged normalized face coefficients. The previous final
footprint is cleared first; rejected candidates force a full rebuild on retry.
The finish kernel obtains unchanged neighbor thermodynamics from the source
cache rather than stale transient metadata.

The next exact activity optimization uses the invariant that source state can
change only inside the previous final footprint. New nonzero velocity/force is
inside that footprint; a new pressure gradient can additionally wake its neighbor
tile. A GPU candidate list therefore covers that footprint plus one tile, with
all other raw activity bits cleared. Seed, page, gate and gravity changes force
one global rebuild. Within pressure iteration, nonzero defects are confined to
the accumulated advection/residual footprint; classification visits that GPU
list and clears the remaining mask bits. This is a dependency bound, not an
activity threshold: small velocities and all material stocks remain authoritative.

The combined footprint path18261fe passes14 native cases, including a20-step
byte-exact dense/sparse comparison across gravity and valve changes, and the
displayed enclosure test. Core and synchronization validation also pass14/14
with `DC_VK_VALIDATE=1 VK_VALIDATION_VALIDATE_SYNC=1`, with no VUID or hazard
messages. Short displayed sweep-group probes remain diagnostic failures:
16x2=23.782/34.348ms,32x1=28.046/46.257ms,128x1=23.776/46.459ms.

The16x2 path now defaults to dependency-bounded setup. Setting an individual
`DC_GAS_SPARSE_RHS`, `DC_GAS_SPARSE_ACTIVITY` or `DC_GAS_SPARSE_CLASSIFY` to0
selects its dense diagnostic path. `DC_GAS_PRESSURE_SWEEPS` and
`DC_GAS_PRESSURE_GROUP` change only submission granularity; convergence still
requires the same tolerance and there is no total iteration/time cap.

Finish and sparse commit are recorded in the pressure submission itself. Finish
is predicated on the reduced residual; commit additionally waits for all candidate
validation and checks the error word. A nonconverged or invalid candidate cannot
modify the working source. This removes one queue-wide completion wait per
substep. All14 native cases and the displayed enclosure pass; the complete
four-run1000-frame displayed battery is now being measured, without validation
or builds overlapping its measured frames. No acceptance result is claimed yet.

The full closed1000-frame runs FAIL:48.938/321.568ms and32.414/258.863ms.
Both have four identical paused captures. The first open run was stopped after
381 frames because sustained cost became excessive; its634.121/1113.589ms result
is diagnostic only, including a brief parser compilation overlap, not acceptance
evidence. All owned benchmark processes were stopped and verified gone before
subsequent builds/tests. No final timing gate has passed.

The permanent `tools/diagnostics/smoke_trace.py` parser has two passing tests and
accounts line-buffered `DC_GAS_TRACE=1 DC_FRAME_TIMINGS=1` records by complete
frame. The open100-frame trace shows30--50 substeps per slow0.25s frame, broad
activity (~180k cells), and substantial transport, pressure and completion costs.
A one-sweep first pass passed14 numerical cases but worsened the open30-frame
probe to773.476/1109.057ms; that default is rejected. Current experiments retain
the normal sweep budget, compile implicit-only shader branches statically, and
wait for a per-gas submission fence rather than queue-wide idle. These remain
under displayed/reference/performance validation and are not a completion claim.

The global advective CFL is now a separate numerical gate. The cold directional
implicit transport oracle passes3 tests, including an independent geometric
packet at CFL4, closed2D wall budgets, and transactional invalid/pause requests.
Its acoustic composition passes5 tests; the existing4 implicit oracle cases
still pass, including a bit-identical acoustic mode at zero bulk velocity.

Experimental `FR_GAS_PRESSURE_LARGE_CFL` uses the same authoritative field and
pressure acceptance test. For large advective steps, frozen Rusanov directional
rates form positive bidiagonal systems. Subgroup affine prefix scans solve
independent lines in parallel, then shared face-flux differences update species,
momentum, pigment and compensated energy conservatively. Gates reset the chain;
the CPU oracle remains validation-only. Small advective steps retain the existing
explicit transport. Invalid candidates halve and retry from the untouched source.

Checkpoint2f82397 passes15 actual displayed native cases. Its new case matches
the composed oracle at CFL4 in one accepted0.4s step with closed budgets. The
smoke demo still selects the previous policy until seam, valve,2D, displayed
enclosure and full performance checks establish the new gate. No speed claim
or final acceptance is inferred from these reference tests.

The independent balanced-counterflow gate subsequently rejected the directional
bidiagonal split: equal opposing velocities artificially compressed uniform gas.
Checkpoint2d663c3 replaces it with an unsplit tridiagonal system per axis.
Normalized matrix-prefix factors and affine forward/backward subgroup scans
solve each line in parallel; conservative shared face differences still update
the authoritative field. The corrected cold transport oracle passes6 cases,
the original implicit oracle4, and actual displayed native tests19, including
analytic stationary density/energy with decaying opposing momentum, chunk and
subgroup seams, closed/open valves and a concave2D wall. This numerical
checkpoint does not establish performance or promote the experimental policy.

Recovery diagnostics retain the experimental selector `DC_GAS_LARGE_CFL=1`;
the default remains policy1. An accepted-step scheduling hint learns rejected
interval sizes and cautiously grows accepted full intervals, without dropping
requested time. Constant-size GPU rejection metadata distinguishes factor,
transport and final-state failures. The real enclosure passes1/1, including
stock budgets, rising finite smoke and pause captures. The full-clock30-frame
probe still fails badly (1404.706mean/2157.514worst ms); retries dominate.

Trace colors exposed subnormal pigment/carrier mismatches. Experimental transport
now requests SPIR-V `DenormPreserve32`, guarded by the actual device property;
unsupported devices retain the old policy. Actual native21/21 cases pass,
including tiny resting and active transported colored stock. Rejections still
occur at a one-smallest-float pigment/carrier difference; no values are clipped
or conservation/reference thresholds changed. The original19-case checkpoint
passed core+synchronization validation; the newer21-case changes still need that
validation. Cached factor coefficients remove duplicate neighbor gathers and
are awaiting new complete-frame timing. This remains unfinished optimization.

Checkpoint896803b instead conditions exhaust, soot and pigment with a shared
power-of-two scale through the line solve and conservative face update, then
unscales only final stored values. Huge-stock lines select scale1 rather than
rejecting or truncating stock. This removes subnormal pigment rejection without
changing the validity rule. A three-pack scan experiment8f77620 was slower and
reverted7dea750. Optional `DC_GAS_WIDE_SCAN=1` and transposed line scratch remain
under measurement; neither is evidence of meeting the frame budget.

The displayed battery now has24 cases:23 pass and one newly added sustained
strong-gravity case remains intentionally RED (9d91d9e). Its20 successive0.05s
steps at kinetic scale1 eventually retry below clock precision. An experimental
Mach-triggered HLLC fallback did not resolve it and was reverted; this is not
classified as a baseline failure. The separate20-step test at prototype kinetic
scale1/512 initially exposed differing cached/full relaxation arithmetic.
GPU row comparison isolated that term; shared explicitly ordered computation
540d75f restores byte-exact equality without changing tolerances.

`DC_GAS_COMPARE_ROW_SETUP=1` reuses transport scratch for cold diagnostic row
baselines and reports only a constant-size GPU comparison summary after the
existing completion. `DC_GAS_TEST_FILTER` and `DC_GAS_TEST_SPARSE_CONTROL` isolate
permanent native cases/controls; an empty test selection fails. New large-step
activity footprints and exact zero-velocity line skipping retain the prototype
byte comparison and the original21 cases. They remain under full-clock timing
and current synchronization validation. The earlier complete headless run had
761 C passes and only the two tracked authored-stage failures in ctr-ojvy;
that result does not turn the new displayed RED into a baseline exception.

The next numerical gate splits large-step energy consistently: transport
half of advective total energy implicitly with Rusanov diffusion, and
retain half plus pressure work in the acoustic enthalpy. This coefficient
is selected before GREEN from the linear acoustic stability bound, not frame
timings: for ideal-gas gamma>=1, implicit pressure coupling B=gamma-1/2 gives
the large-step amplification trace1-gamma/B in[-1,0]. Its determinant tends
to zero, and the finite-step Jury condition2B>=gamma holds. This corrects the
initial trace calculation before implementing the gate. The old zero-bulk/small-CFL
method remains unchanged. A separate cold transport entry keeps the previous
oracle intact; an independent two-cell inverse locks the new energy flux and
closed budget. Native/reference, contact/wave and displayed long-run validation
must establish this experimental gate before any performance promotion.

The half-energy gate passes independent transport and coupled two-cell inverses.
The stiff cold Jacobi reference uses positive damping to remove an oscillatory
roundoff cycle at unchanged residual tolerance. A moving isobaric density
contact then exposed an arithmetic-specific-enthalpy defect: the face flux
created pressure waves at constant bulk velocity. The gate now weights reduced
enthalpy with transported density. Metadata temporarily stores that density and
coefficient in the opposite primitive cache; finish consumes the frozen symmetric
matrix and replaces scratch with final EOS primitives before guarded commit.
The new cold512-cell and actual displayed cross-chunk contact checks pass with
the locked0.1 pressure/0.01 velocity bounds. Current native results are24 passes
and the two sustained strong-gravity failures (implicit and explicit controls)
still RED. The last pre-contact-fix30-frame box probe484.632/1748.961ms remains
far outside acceptance; current core/synchronization and performance validation
are still required. No validity, conservation or prior reference tolerances changed.
