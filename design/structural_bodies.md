# Anchored structural bodies

Status: mechanical reference, GPU placement/topology and GPU equilibrium/contact
coupling implemented; damage, conversion, persistence and gameplay integration
remain planned.
Epic: `ct-fx1b`. The user selected explicitly placed/marked structures and requested
design and tickets first. Ordinary terrain remains terrain. Implementation follows
RED–GREEN–REFACTOR; runtime placement starts with `ct-i5a4` after `ct-touk`.

### Mechanical reference implementation

`include/ferrum/structural/mechanics.h` defines shared synthetic material profiles,
geometry-derived rectangular sections, finite joint tangent modes and capacity
checks. The dense reference solver lives only in `tests/structural/`; runtime
targets never link it. It assembles rotated Timoshenko frame and axial truss
elements, computes support force/moment reactions and rejects rank-deficient
mechanisms. Truss-only nodes omit physically nonexistent rotational variables.
The reference is a linear bilateral oracle; the GPU milestone must add the bearing
active set, finite attachment evolution, rank classification and work-conjugate
contact coupling before runtime support is implemented.

One rigid physics tick is `1/60` simulated seconds; a substep is `1/480` seconds.
The existing rigid velocities and impulses are per tick. Multiply velocities and
impulses by 60 to obtain cell/second and mass-cell/second; divide physical impulse
by the actual structural accumulation interval to obtain force. Tick acceleration
and stress/modulus values multiply by `60^2`. The profiles use mass per unit cell
volume (stone 2.7, wood 0.6, cohesive mortar 1.6) and second-based moduli, strengths
and fracture energies; their tick-equivalent moduli are 20000/6000/1000. These are
game parameters, not measured SI properties. The existing fluid-only variable
duration API does not change rigid tick duration. Numerical member subdivision
preserves summed area-times-length volume and mass, and strength checks accept the
physical unbraced span rather than using the numerical edge length.

### Initial GPU authoring API

`fr_gpu_place_structure` accepts an explicitly authored graph through
`include/ferrum/structural/gpu.h`. Nodes, members, joints and anchors have stable IDs
and generations. Editing rest geometry requires a new generation. Initial placement
accepts rectangular stone/wood members up to 16 cells, with exact rectangular area
and material-derived rigid mass. Initial terrain marking accepts whole horizontal
stone rectangles; sand, irregular regions, water-bearing cells and unavailable
source pages are rejected. Terrain anchor footprints initially accept stone and
carry finite authored stiffness/force/moment capacities; placement validates their
physical proximity to the associated member. Bearing/pinned/bonded/moment types
are stored explicitly; equilibrium and unilateral activation belong to `ct-il9b`.

Placement reserves body slots, validates every source on GPU, then transfers terrain
ownership and activates stationary structural collision proxies. Failure retains
the previous source and graph. Ordinary rigid spawning/removal/motion APIs cannot
overwrite structural ownership. Placement enables the rigid GPU solver. Explicit
authoring commands may synchronously acknowledge validation; ordinary frames update
topology and anchor residency on GPU without CPU graph traversal or readback.
Body-only save/load APIs reject active structural ownership until `ct-1n9n` adds
the graph-aware snapshot transaction; they cannot silently erase graph identity.
Component labels are cached by topology epoch, while anchor material and residency
are checked each frame. Editing joints invalidates the cached labels.

Initial bounds are 32 nodes, 16 members, 64 joints and 32 anchors. One graph stores
each authored joint/anchor once; GPU component labels use the minimum node ID and
cycles do not duplicate roots. The storage shares binding 4 with rigid records,
broadphase, contacts and solver scratch, preserving the 32-descriptor budget.
The extra logical allocation is 122,176 bytes for 64 resident slots or 122,368 bytes
for the 70-slot quarter-native configuration, including reserved solver workspace.
Exact world coordinates are recorded for every uploaded page slot, so recycled
slots cannot become unrelated terrain roots. A cold node or anchor footprint marks
its connected component UNKNOWN. A rooted graph also remains UNKNOWN until the
mechanical solver certifies equilibrium; connectivity never substitutes for strength.

A warm 640 by 448, 70-slot Intel Iris Xe Vulkan diagnostic with all 16 members
and 32 nodes measured 4.545 ms for rigid plus topology work and 14.303 ms for the
whole tick, with validation disabled for timing. The mapped allocation was 64.407
MiB and device allocation 107.196 MiB. This dry benchmark includes no equilibrium
solve; later milestones must measure the additional cost rather than treating
these placement timings as the final structural budget.

### GPU equilibrium and contact coupling

`ct-il9b` adds bounded component-local Timoshenko solves entirely on Vulkan. Each
component uses at most 96 displacement variables, eight unilateral/friction active
passes and three physical-residual refinement sweeps per pass. Finite oriented
anchor and joint interfaces transmit forces and lever-arm moments. Bearings open
in tension; sliding traction obeys Coulomb friction and requires compression.
Rigid position and velocity corrections accumulate work-conjugate loads at their
actual contact points, once per applied increment, over the fixed physics tick.
Self-weight uses the shared material density. Structural proxies remain stationary;
the solved displacement is an internal diagnostic, not an animated pose.

Explicit node/member/component readbacks expose stress, utilization location,
reactions, force/moment budgets, convergence counters and reason flags. Ordinary
frames perform no CPU solve or graph readback. Cold boundaries, clipped/overflowed
contact loads and numerical nonconvergence produce UNKNOWN. A rank failure becomes
UNSUPPORTED only when a separate geometric mechanism certificate succeeds.
Independent components retain their own classifications.

Seventeen equilibrium tests cover symmetric/eccentric passengers, angled braces,
finite interfaces, unilateral and sliding bearings, stress overload, cold pages,
contact overflow and weak numerical pivots. The full headless battery has 181
passing tests; quarter-native and UI batteries pass with Vulkan validation. One
earlier quarter-native run crashed in the existing concave-body visual executable;
the full retry, standalone, debugger and ASAN/UBSAN runs passed without reproducing
it. This is an unresolved intermittent observation, not a claimed crash fix.

A warm dry 640 by 448, 70-slot diagnostic with 16 independent members measured
6.979 ms for rigid/topology/equilibrium and 17.335 ms for the whole tick, with
validation disabled. Allocations remain 64.407 MiB mapped and 107.196 MiB device
storage. These are stage measurements, not the final loaded gameplay budget.
The 32-node, 16-member chain sweep safely returns UNKNOWN when float conditioning
is insufficient, rather than inventing a mechanism. Current Euler checks use the
conservative per-member span twice its length; complete physical unbraced-span
and material grain evolution remain part of the later failure milestones.

## Behavior and ownership

Introduce a structural body kind alongside terrain and freely moving rigid bodies.
A player can place a structural member or explicitly mark eligible existing solid
geometry as structural. Marking is an atomic transfer of ownership, not a second
overlapping copy. Loose sand/water cannot be marked as a beam. Weak cohesive solid
profiles may be added explicitly without redefining ordinary dirt terrain.

Structural members have world-space rest geometry, materials and explicit joints.
They collide with and hold up rigid bodies. While stable, their collision geometry
stays at its anchored rest pose; small elastic displacements are analysis state,
not a second independently moving collider. Large deflection invalidates this
approximation and enters a documented instability/failure path. The first model
is a 2D frame with unit out-of-plane thickness, matching the current simulation.
It does not claim 3D torsion or out-of-plane buckling.

Rigid, structural and terrain ownership are exclusive. A structure is a collection
of material-bearing members, not a material ID or a caller-supplied kinematic flag.
Collision proxies may use the existing rigid occupancy/contact pipeline, but retain
structural identity and material-derived mass even when their contact inverse mass
is zero. Solver state and damage remain GPU-owned during ordinary frames.

Terrain attachments have a footprint and finite capacity. A supporting soil patch
is not an infinitely strong welded root. Removing/eroding a footprint invalidates
its attachment. Contact with a floating rigid body cannot become a terrain root.
Unknown terrain across an unloaded chunk boundary is not proof of missing support.

## Mechanical graph

Graph vertices are physical junctions and member endpoints, not just one centroid
per collision body. Each vertex has in-plane displacement `(u_x, u_y)` and rotation
`theta`. Graph edges represent member sections or authored joints. A member can
contain several elements; section cuts and graph topology must correspond to its
actual material volume and authored shape. Contact loads map to these elements
through work-conjugate shape functions, preserving force and moment.

Initial members use a 2D beam/frame formulation with axial, shear and bending
response. Prefer Timoshenko elements for appreciable shear deformation; recover
Euler–Bernoulli behavior in the slender-beam limit. A pure axial truss element is
appropriate for pin-ended braces, not for a beam that must carry bending. Broad
panels and irregular marked regions need a geometry-preserving plane-stress
discretization or a separately validated equivalent section model. They cannot be
silently replaced by slender bars. The first placement UI may restrict accepted
member forms until that discretization is available.

Store length, area `A`, second area moment `I`, shear correction, section extreme
fiber distance `c`, orientation and material grain direction. These derive from
geometry and the common unit convention. Numerical subdivisions must not add
volume, mass, strength or extra anchors. Collision decomposition and mechanical
discretization are separate mappings to the same material-bearing geometry.

| Connection | Mechanical constraints and limits |
| --- | --- |
| Bearing | Compression-only normal reaction and friction-limited shear; no adhesive tension or fixed moment. |
| Pinned joint | Transfers allowed axial/shear force, releases relative rotation; finite connector strength. |
| Bonded joint | Finite normal/shear and rotational stiffness; tension, shear and peel/pull-out capacities from bond area and material. |
| Moment-resisting joint | Transfers force and moment through an explicit attachment/section; capacity is not infinite. |
| Terrain anchor | One of the above constraint types, tied to validated world-cell footprint and embedment geometry. |

Geometric touching alone creates bearing, not a weld. Joining adjacent members
requires explicit attachment intent and a valid shared interface. A diagonal brace
helps because it resolves force into an axial load path and restrains a mechanism;
a sideways cantilever needs a connection capable of resisting its root moment.
A pinned cantilever with no brace is unstable even though its graph reaches terrain.

## Equilibrium and strength

For small deformation, assemble element/joint contributions into `K q = f`, with
anchor constraints and an active set for compression-only/friction-limited contacts.
Transform local element matrices into world coordinates. Balance both force and
moment at every junction. Use stiffness-based load sharing through parallel paths;
cycles must not circulate or multiply a scalar support score.

Recover local axial force `N`, shear `V` and moment `M`. Useful section checks are:

```
sigma_minus = N/A - M*c/I
sigma_plus  = N/A + M*c/I
tau        = section_shear_factor * abs(V)/A
P_cr       = pi^2 * E*I / effective_unbraced_length^2
```

Separate tensile, compressive and shear allowables and connector capacities.
Define utilization from the governing material interaction rule, including
combined axial/bending stress, shear, bearing/pull-out and compression instability.
Euler buckling is an initial slender-member check; end restraint and effective
unbraced length come from actual joints/bracing, not a constant per graph edge.
Bracing must not gain fictitious strength merely by subdividing a column.
Short/non-slender sections need their applicable crushing/instability model.

Central material profiles contain density, elastic/shear properties, distinct
strengths, fracture energy, damage/grace parameters, failure mode, and optional
grain/moisture/rot state. Wood is direction-dependent. Stone is much weaker in
tension than compression. Synthetic game coefficients must be labeled and
calibrated against analytic tests; stiffness and ultimate strength are different
parameters. Root capacities include both structural and terrain interface limits.
Define explicit mappings between terrain IDs, rigid-body material IDs and structural
profiles: current body wood and terrain sand use different namespaces. Marking
stone or transferring cohesive material/particles must not reinterpret numeric IDs
or leave a second MPM copy. Placed wood can use its body material profile before a
wood terrain-cell representation exists.

Use cells, material mass units and a documented simulation-time unit. The existing
rigid solver uses tick-normalized substeps; explicitly convert its reaction impulses
and gravity to the structural unit convention before sharing loads. Do not insert
SI moduli or use wall-clock frame duration alongside tick-unit forces. Damage and
grace timers consume the same authoritative simulated seconds used by physics.

The constitutive and stability basis follows MIT's
[beam/plate constitutive notes](https://ocw.mit.edu/courses/2-080j-structural-mechanics-fall-2013/32670f14cec210d98c5c7fe9dbf73eb6_MIT2_080JF13_Lecture4.pdf)
and [elastic stability notes](https://ocw.mit.edu/courses/2-080j-structural-mechanics-fall-2013/5a7038cb6ca0db1dda868e9d669b6836_MIT2_080JF13_Lecture9.pdf).
The bounded GPU graph, gameplay damage law and transitions below are project design
choices, not claims that those sources describe this runtime.

## Rigid-body coupling and pass order

Self-weight is a distributed structural load. Rigid passengers contribute their
actual contact reactions, including contact-point torque. Do not also add their
mass as structural self-weight. A collision body's structural proxy stays still
while the supported rigid body participates in the existing XPBD solve.

Capture effective applied constraint impulses at each contact. Position multipliers
require timestep conversion and the actual Jacobi relaxation/application weights;
raw accumulated multipliers alone are not necessarily the applied reaction. Position
correction already contributes to derived rigid velocity. Count restitution/friction
velocity impulses only as additional applied momentum, not as a duplicate of that
position correction. Validate summed contact impulse against rigid momentum change
after removing gravity, explicit forces and independently reported clipping losses.
Average impulse over the structural interval to obtain sustained force; preserve
impact peak/work separately for brittle damage. Never confuse force with impulse.

Per fixed simulation tick:

1. Commit validated placement/removal and terrain-footprint edits; update dirty
   graph connectivity and residency. Preserve stable IDs and generations.
2. Run rigid substeps against current structural colliders and capture their net
   force/moment/work contributions on GPU.
3. Solve current graph equilibrium/stability with self-weight and contact loads;
   recover stresses, anchor reactions and residuals. Submerged loads join this
   stage once the fluid-coupling milestone is complete.
4. Advance confirmed overload/support-loss time, damage and failure candidates.
5. Reserve and commit bounded joint cuts, island relabeling and output conversions.
   Queue dependent rigid wake-up for the next substep/tick. Structural load and
   failure decisions lag by at most one fixed tick; test that bound explicitly.
6. Publish final occupancy to fluid/material passes and render stress/damage state.

A bounded GPU iterative solver with block preconditioning is the starting candidate;
its iteration cap and update interval must be selected from quarter-native benchmarks
and analytic convergence tests. Warm starts are invalidated by topology changes.
Anchor connectivity is necessary but insufficient: distinguish real rigid/hinge
mechanisms from poorly conditioned systems or exhausted iterations. A numerical
residual failure is `UNKNOWN`, never proof of structural overload. No artificial
regularization may make an unsupported mechanism count as anchored.

Runtime states include `SUPPORTED`, `OVERLOADED`, `UNSUPPORTED`, `UNKNOWN` and
`PENDING_CONVERSION`. Missing constraints, overflow and unclassified nonconvergence
freeze destructive decisions for the affected component; other known components
continue. Detached/unstable states require a mechanical classification, not just
failure to converge. Diagnostics expose residual, force/moment imbalance, rank or
stability classification, capacities, dirty work and GPU stage time.

## Delayed failure and material response

Use simulated-time damage rather than a frame counter or rendering delay. A starting
gameplay law for confirmed overstress is:

```
dD/dt = max(utilization - 1, 0)^p / overload_time
```

`D` is permanent material damage, bounded to `[0,1]`. Unloading stops further
damage but does not silently heal a crack. Lowering stiffness/remaining strength
must use a documented softening law and a fracture-energy/section-length treatment
so graph refinement does not change failure energy. Avoid singular stiffness before
an atomic cut can commit. Real creep/fatigue is a later material model, not an
unlabeled property of this simple law.

Confirmed unsupported or mechanism components also carry an unsupported-time
accumulator with a material/gameplay grace duration. Recovery before the grace
expires resets that countdown; already accumulated material damage remains.
`UNKNOWN` does not advance either timer. Idle wall time, FPS and chunk eviction
cannot accelerate collapse. Mark unsupported grace visibly as impending collapse;
it is a gameplay delay, not a fictitious equilibrium solution. Severe impact can
consume the damage budget quickly through the impact fracture rule.

At failure, cut failed member sections/joints and recompute components. Rooted
survivors remain structural and receive redistributed loads on the next solve.
Unrooted survivors become dynamic rigid assemblies. Preserve still-sound internal
connections: losing the terrain root does not itself imply pulverizing the material.

| Material/state | Intended output |
| --- | --- |
| Sound member with failed attachment | Intact falling rigid member/assembly. |
| Wood with section/grain failure | Coherent split pieces along the modeled grain; sound pieces fall rigidly. |
| Stone with tensile/bending fracture | Bounded rigid stone fragments preserving remaining sound volume. |
| Severe stone crushing | Smaller rigid fragments and/or accounted granular stone debris. |
| Weak cohesive soil/mortar-like profile | Material-appropriate crumbling MPM debris. |

Mass, occupied volume, material composition, linear/angular momentum and water
carried by debris are budgeted through conversion. Dissipation of elastic energy
and fracture work is explicit. Do not turn virtual small-deflection analysis energy
into an arbitrary launch impulse. Map velocities from physical state according to
the selected rigid/structural coupling model and report remaining dissipated energy.

Conversion is reserve–validate–commit with stable transaction identity. Reserve all
body, shape, graph and MPM outputs before deleting any parent geometry or bonds.
If any pool is full, retain one authoritative pending source and retry; never leave
an invisible or duplicated supporting collider. Preserve concavities and mixed
material composition. Current four-piece, uniform-material rigid records may need
extension or sound welded multi-body assemblies; do not fill holes or average away
volume to fit them. A detached composite's mass/density still derives from materials,
and its buoyancy from actual displaced water, following `dun-spbq` and the existing
stone/wood coupling tickets. Dry support and failure must be implementable without
waiting for the entire water pipeline.

## Streaming, persistence and scale

Persist structure/member/joint IDs, exact rest/collision geometry correspondence,
world anchor cells and footprint generations, material/grain/state, damage,
unsupported time, cut history and pending conversion identity. Save these alongside
the rigid/world transaction; derive transient matrices and graph labels on reload.
Persist damage on cross-chunk edges once under a canonical stable owner.

Rebasing changes only viewport coordinates. Page-slot reuse cannot change anchor
identity. When a graph crosses cold chunks, cached state is authoritative but the
unresolved mechanical boundary is `UNKNOWN`; do not pin it to an imaginary rigid
root or call it empty space. Freeze destructive evolution for that component until
its constraints are available. No offscreen damage fast-forward without validated
load history. Reopening during a failure must neither reset the grace nor duplicate
outputs. Worlds without structural records remain compatible.

Separate node/edge/element capacities from the existing 64 rigid slots; choose
initial limits after profiling rather than multiplying descriptor bindings or
allocating per-frame memory. Store graph state in an existing packed buffer or
rebudget descriptors explicitly: this machine already uses 32 storage bindings.
Use dirty adjacency updates, resident component work lists and bounded iteration.
No normal-frame CPU graph traversal, global solve or GPU readback. Test CPU reference
solvers are oracles only. Saturation, omitted work and nonconvergence are visible
diagnostics with safe rollback/frozen damage, never hidden material deletion.

## Ticket roadmap and review gates

| Order | Ticket | Deliverable |
| --- | --- | --- |
| 1 | `ct-touk` | Mechanical model, shared material profiles and analytic CPU test oracle. |
| 2 | `ct-i5a4` | Explicit placement/marking, GPU topology and finite terrain anchors. |
| 3 | `ct-il9b` | GPU equilibrium, instability classification and rigid reaction coupling. |
| 4 | `ct-0iq3` | Timed support failure and atomic conversion of unsupported islands. |
| 5 | `ct-gf6q` | Material-specific splitting, cracking and crumbling. |
| 6 | `ct-1n9n` | Streaming/save/reload of graph identity, damage and transactions. |
| 7 | `ct-ha8q` | Interactive quarter-native visual battery, water handoff and budgets. |

Stages 5 and 6 can proceed independently after stage 4. Water integration reuses
`dun-c459`, `dun-08qq`, `dun-sbk9` and `dun-spbq`; impact fracture shares conversion
infrastructure with `dun-75oh`. Link those tickets rather than creating separate
sink/float toggles or incompatible fragment ownership schemes. Epic completion
requires every implementation milestone and the final visible acceptance battery.

Each code ticket starts with compiling failing tests and a RED commit, then GREEN
implementation and REFACTOR with full regressions. Planned analytic gates include
cantilever root shear `F`, moment `F L` and slender-limit deflection `F L^3/(3 E I)`;
symmetric/eccentric bridge reactions; rotated angled frames; pinned mechanisms;
multi-path load sharing; compression/tension asymmetry; buckling and refinement
invariance. Test member/joint failure separately from attachment loss.

The paced visual battery must show two different concave rigid bodies held above
terrain by a sideways beam and angled brace, then remove the brace and show delayed
failure and falling loads. Compare sound detachment, wood splitting, stone cracking
and weak-material crumble. Include load overlays, anchor edits, cold seams, save
mid-countdown, saturated output pools, transient impacts, equal simulated time at
different FPS and submerged wood/stone outcomes. Integrate it into `test_ui` and
`test_quarter_native`; retain exact settled-frame checks and Wayland hide/restore
tests. Record GPU timestamps, memory/capacity, conservation and validation results.
