# Fire, smoke and reactive materials

Status: implementation authorized after the initial design-and-ticket turn.
Epic: `ctlo-d7vm`. Runtime progress and unfinished work are tracked in its tickets.

## Practical interactive numerical policy

Latest user direction supersedes earlier exact per-frame convergence, bitwise
identity and perfect-energy requirements. Diagnose delayed stalls with frame
time series, not only averages. Gameplay pressure work must be bounded; it must
not wait indefinitely for a residual or retry negligible roundoff indefinitely.
Use finite, nonnegative state, convincing visible flow and measured long-run
inventory/energy drift as acceptance. Exact-reference and dense-parity modes
remain diagnostics. Negligible subnormal/pigment and tiny-negative cleanup is
allowed and must be documented. This does not authorize erasing meaningful
smoke/fuel/products, dropping simulated time, changing resolution or inventing
a performance pass. Supported gameplay parameters and unresolved extreme-force
diagnostics must be distinguished at admission. The complete-frame target and
sustained actual Vulkan/Wayland validation remain required, including the
delayed onset, retries, waits, activity expansion and resource growth.

The experimental `DC_GAS_MULTIGRID=1` practical pressure backend uses one fixed
h1/h2/h4/h8 V-cycle: twelve red/black fine sweeps before and after, four weighted
Jacobi sweeps before and after at h2/h4, and thirty-two coarse sweeps at h8.
Restriction sums residuals and boundary conductances (piecewise-constant
Galerkin aggregation); it refuses to aggregate through a closed face or invalid
cell. Prolongation adds the parent correction without crossing those barriers.
All hierarchy storage reuses GPU transport scratch; no field readback, residual
retry, extra submit or physical-time reduction is involved. Intervals above
50 ms retain the established bounded forty-eight-fine-sweep path: the reduced
work cycle failed the long-interval stability test and is not admitted there.
This opt-in backend must pass sustained displayed measurements before promotion.
Its pressure activity threshold is a relative right-hand-side disturbance of
`1e-3` (0.1 percent of the local pressure-energy scale); strict and long-interval
fallback paths retain `1e-7`. This defers imperceptible corrections rather than
iterating to exact convergence, while preserving full elapsed time and stock.
The optional fused metadata/RHS path reads an immutable transported candidate
for all neighbor predictors. Each owning cell retains full EOS validation;
buoyancy is applied during finalization before the same guarded commit. Kinetic
guards can skip their scaled square roots only below a conservative 98-percent
energy comparison; the existing limiter handles the boundary and extreme values.
`DC_GAS_HALF_PRESSURE=1` is a separate experimental storage backend for the
validated sweep schedule. It rounds only derived off-diagonal coefficients down
to half precision, keeps the right-hand side and reconstructed diagonal in full
precision, and retains the original capacity term when rebuilding that diagonal.
The authoritative gas state remains full precision. Unsupported coefficient
ranges fall back per row to the original storage. The sixteen-byte rows occupy
unused transport scratch and require one GPU packing pass. This is approximate,
so stock/flow/stability checks replace byte identity for this option; exact
storage and exact-reference modes remain available.
Standalone physics ticks use their own Vulkan completion fence. They wait for
all recorded physics work before gas/capture handoff, without draining unrelated
presentation-queue operations. Stepping and capture retain the same kernel
schedule and elapsed-clock accounting.

## Modular delivery sequence

Latest user direction: develop one visually testable gameplay feature or system
at a time, with the prerequisites that feature actually needs. This takes
precedence over completing every broad foundation ticket before a visible result.

There is one active visible milestone. Implement, demonstrate, test and push it
before starting the next. Small prerequisite changes belong to that milestone;
do not expand them to every future material, owner or scenario. Keep thermal,
gas, reaction, ownership and renderer adapters as separate modules with explicit
interfaces, rather than growing a shared simulation monolith.

| Delivery order | Ticket | Runnable result |
| --- | --- | --- |
| Next | `ctfs-2hu1` | Heat/wetting bench: finite heat source/brush, wood/stone, water cooling, and visible temperature/wetness feedback |
| Then | `ctfs-dmxv` | Smoke/ventilation box: finite colored smoke, real enclosure/vent controls, flow and oxygen feedback |
| First gameplay feature | `ctfs-81vw` | Terrain wood bonfire: ignition, gradual fuel/char/ash, visible flame/smoke, warmth and dry/wet containment controls |

These are functional systems, not cosmetic substitutes for missing simulation.
Each needs a permanent demo/test entry, real Vulkan/Wayland captures, appropriate
reference/conservation tests and complete-frame measurements. Record its exact
supported interactions and prerequisites. Do not enable an interaction that loses
authoritative state or disable existing mechanics to make a demo appear correct.
If a supported interaction requires more persistence or handoff work, complete
that specific prerequisite then; unrelated future paths can remain open.

The broader technical tickets below retain their full acceptance criteria and
stay open until actually complete. The new milestones link to them rather than
pretending a terrain heat bench completes all heat/gas/persistence requirements.
Finish or preserve the current in-flight checkpoint work at a tested safe
boundary, document pending intentional RED cases, and stop broad DCS/analytic
body/actor/chemistry expansion until the active visible feature needs it. No work
is discarded or hidden in a stash.

After the bonfire, select one visible extension at a time, such as a cold pivoted
launcher, coal siege damage, a sludge/water basin, or sticky Greek fire. Each adds
its own necessary prerequisites and demo before moving on. The final three large
integration scenarios and strict frame budget remain required; early slices
must not be mislabeled as completion of the epic.

## Intended behavior

A contained wood-and-stick bonfire warms nearby actors without inevitably
igniting the forest. A sound, assembled catapult can launch burning coal into a
dry wooden wall, causing progressive damage while leaving a thick wall's core
and the launcher intact after one shot. A wet control wall resists ordinary
ignition. The same launcher can fire burning oil/red-slime-jelly compound: Greek
fire adheres, liquefies when heated, consumes fuel faster, survives water dousing,
corrodes valuable matter and can ignite a launcher that holds it too long.

Fire is a reaction and heat source; flame appearance is its output. Smoke is
advected gas and suspended soot, with composition and density. Wetness, coatings,
electrical exposure, oxidation and structural damage are separate persistent
state. Magic may change reaction rules and colors explicitly.

The implementation must retain Vulkan, GPU-parallel work, dynamic body storage,
authored colors, arbitrary connected components and existing ownership semantics.
All visible simulation and rendering stay 1:1 at 480x270 for the current frame
acceptance target. The 640x448 rounded allocation includes the chunk halo.
Native 640x360 is a separate open performance target. No resolution or mechanical
substep reduction may be used to pass this feature's performance checks.

## Existing systems and missing pieces

Water volume lives in `dc_cell_t.fluid_mass`; face velocity drives conservative
transport. MPM currently handles granular particles, including bound moisture.
Neither is a smoke/oxygen solver. Leaves already use particle flag bits for
their authored color; those bits cannot be reused for chemistry.

Terrain, rigid pixels and loose particles have different owners. Loaded terrain
stays fixed until a modification disconnects it. Burn removal must count as such
a modification; loose ash or leaf debris must not become fixed to a newly
extracted body. Copying geometry or moving a camera cannot reset burning/wetness.

The structural graph has static pinned attachments and equilibrium constraints.
It does not supply a moving rigid-body revolute joint for a catapult arm. The
current rectangular structure authoring and incomplete generic authored staging
also remain limited; the first siege fixtures can use supported programmatic
parts, while general asset admission remains tracked in `ctpf-iueo`/`ctr-ojvy`.

## Material profiles and state ownership

Profiles declare density, heat capacity/conductivity, ignition hazard, ignition
temperature, burn rate/energy, oxygen demand, char/ash/soot/gas yields, moisture
capacity, wetting/drying, electrical conductivity and per-reagent susceptibility.
Oil/slime additionally declare yield stress, shear response, thermal softening
and adhesion. Composition and material mapping stay explicit across terrain and
rigid namespaces. Existing IDs are not renumbered.

Wood, dense wood, leaves, leaf bits, coal/char and oil are fuel-bearing profiles.
Sticks can initially be thin wood geometry: exposed surface-to-volume ratio
already distinguishes them from a beam. Stone is a nonfuel fire-pit material;
an oil film on stone can burn without making stone itself fuel. Ash is a new
granular residue, distinct from dirt and leaf bits. Red-slime jelly, Greek-fire
compound and magical corrosion residue need explicit identities. Coin and
ingredient test materials are added with their relevant conversion rules.

Dynamic sidecars hold thermal energy, remaining fuel/char fraction, bound water,
coating composition/amount, oxidized/corroded fraction, ignition-hazard progress
and any bound oxidizer. Do not extend the overloaded particle flags or confuse
the loaded-terrain bit with flammability. A material can have combustible coating
and a noncombustible substrate simultaneously. Rust does not universally increase
flammability: each substrate/reaction profile defines the effect.

| Owner | Authoritative address and transfer |
| --- | --- |
| Terrain | World chunk coordinate and local cell, with slot generation checks |
| Rigid material | Stable material parcel/rest texel, body ID/generation and arena mapping |
| Loose granular material | Stable particle identity, both alternating MPM pools handled |
| Bulk oil/slime | Conservative per-cell phase volume/composition and face flux |
| Thin coating | Host surface/rest coordinate, amount and composition; debited from bulk |
| Smoke/air | World-cell gas species mass, energy and gas face velocity |

Extraction, carving, fracture, melting, ash birth and coating detachment preserve
parcel identity and transfer state through the existing reserve/publish boundary.
Mass, energy, pigment and sampled ignition thresholds are not recreated on owner
changes. On capacity failure, retain source state and retry; do not consume fuel
without admitting its residue/products. Use growable arenas and conservative
backpressure, never a fixed global number of fires, particles or bodies.

Versioned chunk/body/particle/coating/constraint snapshots persist these channels.
Old saves initialize ambient thermal state and zero reaction progress, preserving
existing bound water, material, colors and ownership. Reject corrupt/newer schemas
before publication. Streaming must not relight, replenish fuel or freshen cave air.
Optional exporter channels declare initial wetness, coatings and thermal state;
material tables provide defaults. Visual animation never replays a chemical spawn.

## Ignition, heat and material loss

Ignition is conditioned on heat/ignition exposure and fuel, then modified by
wetness, coating, accumulated shock energy, material state and oxidizer supply.
For hazard rate lambda, the interval probability is `1 - exp(-lambda * dt)`.
Sample a stable parcel/event threshold once and accumulate hazard in simulation
time. Do not reroll every rendered frame or seed by resident slot/body index.
Equal-time scheduling tests, seeded populations and owner changes must preserve
the intended distribution and reproducibility.

Use a data-driven progression: unburnt -> heating/drying -> char/smouldering or
flaming -> residue/cooling. Char can remain fuel; ash cannot spontaneously regain
wood fuel. Exposed geometry and heat penetration limit burning to a progressing
front. No whole connected component converts to ash merely because one pixel lit.
Chance governs ignition; fuel/oxygen/heat budgets bound subsequent reactions.

Heat is stored as energy with profile-dependent temperature. Pairwise conduction,
gas convection, contact exchange and bounded radiative exchange transfer energy;
cooling to the environment is an explicit sink. Water absorption/evaporation
debits/credits water and vapor and consumes latent heat. Normal water dousing
cools and wets fuel and limits oxygen access; enough sustained exposure can
eventually dry it. Maintained very-wet controls resist ordinary fire. Shock is
an energy input, not a universally flammable boolean.

Each reaction debits finite fuel plus external or bound oxidizer, and credits
ash/char/soot/vapor/exhaust according to a normalized mass ledger. Oxygen incorporated
into exhaust counts in product mass. Heat release is capped by reacted fuel's
chemical energy. Inert smoke is not charged a second oxygen-consumption cost.
Competing surfaces reserve from the same local oxygen budget before commit.

Shrinking solid volume changes mass, center of mass, inertia, contact geometry
and structural section/strength. Recompute affected quantities, wake affected
contacts/supports and pass unsupported remnants to the existing fracture rules.
Valid mechanical grace periods remain; fire cannot erase or pin entire assemblies.
Ash retains its own heat and reaction/pigment state as loose particles.

## Smoke, oxygen and gas motion

Add a separate, variable-density, weakly compressible gas field in available air
volume. Species include oxygen, inert carrier/exhaust, water vapor and suspended
soot, with extendable magical pigment/reagent channels. Gas density is far below
water density; buoyancy derives from mixture density and heat. Cooled heavy smoke
need not always move upward. Flow mixes sideways, around bodies and through real
openings; stone, liquid occupancy and closed boundaries constrain its apertures.

The first gas ticket must compare a bounded weakly compressible pressure law
against a small reference solver, including sealed-volume storage, heating,
expansion, cooling and CFL stability. Select game-unit sound speed/relaxation
and substep policy from those tests before GPU production integration. This is
an explicit numerical decision gate, not permission to reuse water's zero-divergence
projection and call it compressible smoke. A low-Mach expansion-aware projection
is an alternative only if it meets the same sealed-volume and frame-cost contract.

Conservative face flux transports species mass, energy and pigment. Differently
colored plumes mix pigment by admitted carrier/soot amount instead of overwriting
the destination color. Smoke-rich
products lower oxygen fraction and combustion consumes oxygen. Injecting inert
smoke can dilute/displace breathable air, but must not silently delete oxygen
inventory; dilution and chemical consumption have separate diagnostics. Oxygen
starvation suppresses ordinary flames and can leave hot embers that reignite after
ventilation. Steam has its own mass and heat, rather than being cosmetic smoke.

Ambient replenishment occurs at explicit world/sky/vent boundaries. Camera edges,
unloaded pages and newly reused slots are not oxygen sources or smoke sinks.
Cold pages retain state and block unresolved flux. Submerged product gas needs
finite compressed-pocket retention and pressure-driven liquid displacement so
Greek fire can continue reacting under water. This functional coupling is part
of the milestone; bubble artistry can follow later. Neither gas nor water may be
deleted, and ordinary flames gain no oxidizer from water.

Sparse GPU tiles activate from heat, reactions, changed solids, wind and nonambient
gas. Neighbor halos wake before flux. Tiles sleep only after conservative settled
state has been retained; visible density is not discarded to satisfy a work quota.
The gas solver uses its own velocity/pressure and does not overwrite water faces.

## Oil, slime and Greek fire

Bulk oil is continuous volume with profile-derived density, finite receiving
capacity and a distinct phase alongside water. Its density is a gameplay profile
value, not an unconditional float/sink flag. The required dense sludge must resist
small loads and shear more readily under sufficient stress; use a regularized
yield-stress/shear-dependent constitutive model. Heated slime/oil softens and
flows; cooling increases its resistance again.

Continuous volume/composition is authoritative. MPM samples guide stress/momentum
and deformation, with quadrature mass derived from that volume; they are not a
second independently conserved supply of oil. Conservative volume flux owns
transfer. Reference tests reconcile every MPM transfer/resample against the phase
ledger. Do not route bulk sludge through the granular two-slot arrival policy.
Water/oil share free-space capacity, exchange drag and heat and displace each other
according to density/rheology. Neither receives a free full cell independently.

Coating transfer removes the same amount from bulk and deposits it on terrain,
rigid surfaces, foliage or a future actor's surface attachment interface. Coatings
follow the host's rest coordinates, can drip/tear under force and redistribute
through fracture. Collision/contact wetting supplies attachment; there is no
global search over every coated object or flame. Monster integration first uses
a moving damageable proxy and stable body/entity generation, not a new wildlife AI.

Greek fire is the fictional reaction profile of oil and red-slime jelly. Mixing
consumes the real input quantities and creates no free fuel. Its adherent hot film
burns faster than the matched ordinary oil control, has a finite bound-oxidizer
budget, and does not use ordinary water-dousing cancellation. It can continue
reacting while doused/submerged until fuel or its bound oxidizer is exhausted.
Water still exchanges heat, momentum and volume; it is not erased or turned into
an unlimited fuel source. Energy and product accounting remain finite.

Its corrosive reaction converts affected substrate into specified low-value
residue. Magical gold corrosion explicitly yields the game's rust-like residue;
burned magical ingredients yield spent ash. Conversion consumes original item
mass/potency and persists provenance so picking up, unloading or reimporting the
residue cannot recover pristine coins/ingredients. Unaffected remaining matter
can still be reclaimed. Ordinary oxidation does not grant this magical rule.

Volatility emerges from finite reaction rate, adhesion/leaks and heat reaching
nearby launcher wood. The normal quick-launch fixture is safe; a delayed ignited
load has a high seeded ignition probability. Do not use a timer that ignites any
catapult regardless of shielding, wetness or contact. A genuinely fireproof carrier
must remain a viable player construction.

## Siege mechanics, presentation and future hooks

The catapult consists of supported constructed frame/base parts and an independent
rigid arm attached by a two-body revolute/pivot constraint. It constrains the two
anchors' relative translation while permitting rotation, with finite limits,
friction, load capacity and generation-safe save/load. Release transfers the
projectile through an actual latch/contact transaction; position/velocity cannot
be teleported to fake a throw. Drive/work or stored spring energy is accounted for.
Recoil, off-center load and repeated operation must not collapse a sound frame.

Fire emission aggregates into the existing emissive/directional light buffer;
never scan every individual fire pixel as a point light. Smoke contributes colored
extinction and scattering independently of its fuel/temperature. Magical flame
color does not silently change heat release or oxidizer rules. Visible emissive
fire and smoke must work while propagated lighting remains behind its current
flag. Enabling lighting is measured separately and cannot disable physical heat.

Future lightning injects deduplicated energy/shock events through material-defined
conductivity, including connected conductors and water. This roadmap provides the
profile, exposure and damage interfaces; lightning bolt generation and full
electrical network propagation are future work, not prerequisites for a campfire.
Warmth/exposure and fire/smoke avoidance fields are exposed for future wildlife.
Those hooks are tested with probes; no nonexistent wildlife behavior is claimed.

## Frame order, diagnostics and performance

Use immutable input state, GPU proposals/reductions, capacity/oxidizer reservation
and committed output. Define the heat/reaction/transport split and publication
barriers before implementation. Reaction intervals come from accepted simulation
time, with explicit adapters for the current fixed rigid clock and timed-fluid
API. A render, an idle presentation or each of eight rigid substeps must not
accidentally charge another full reaction tick. Paused frames are chemically idle.

Heat changes and burn removal wake affected mechanical work; final geometry and
apertures feed gas/liquid transport, and output fields feed rendering. Cross-phase
exchanges must be equal-and-opposite and each owner publishes once. Diagnostic
readbacks are optional; no frame-time CPU component solve, fire list construction,
whole-component serial traversal or particle/light scan is permitted.

Record full frame time, per-pass GPU timestamps, active tiles/owners, reaction
counts, conserved inventories, boundary fluxes, allocation growth, pressure/CFL
limits and retained overflow. Emit reproducible fixture manifests and actual
screenshots/captures into the repository's existing diagnostic workflow. Tools
are permanent repository tools, not throwaway external scripts.

Lock a pre-feature baseline and acceptance fixture settings before implementation.
At 480x270 physics/render on the actual Iris Xe/Wayland display, each final
scenario needs two consecutive 1000-frame measured runs with complete-frame mean
and worst below 30 ms, with no concurrent build/test and no reduced physics clock.
Report startup/compilation and sustained simulated gameplay separately. Normal
display timing and seeded long-simulation runs are both required; headless-only
evidence cannot close rendering/performance tickets. A failed budget remains open.

## Required integration scenes

The following are initial fixture specifications. Baseline work must lock exact
maps, seeds, profiles, doses, timings and numeric tolerances before GREEN work;
changes require a recorded balance/design revision rather than threshold loosening.

| Scene | Positive case and explicit controls |
| --- | --- |
| Forest bonfire | Wood pile and thin sticks among nonfuel stones; cleared ring and nearby dry/wet forest. Sustain a useful warmth band for 180 simulated seconds, consume finite fuel and produce char/ash/smoke. Across still air and locked gentle crosswinds, zero ignition beyond the cleared boundary. Removing the ring or bridging it with dry fuel must permit spread, proving containment is physical. Warmth and avoidance probes work without implementing wildlife. |
| Burning coal siege | Supported assembled launcher, pivoted arm, real release and a single burning coal ball versus an 8-cell-thick dry wood wall. Lock impact energy and flight path. Initial target: 10-35% wall fuel/mass loss after 60 simulated seconds, with localized char and a surviving load-bearing core; no immediate through-wall deletion or sound-launcher collapse. Same geometry under maintained high wetness has no sustained ordinary ignition. Repeated shots may eventually breach it. |
| Greek-fire siege | Same mechanism, matched oil and Greek-fire controls. Hot compound adheres to moving foliage/actor proxies and burns under water dousing. At matched initial fuel inventory, target depletion time at most half the ordinary-oil control. Coins and ingredients become finite rust/ash with lost pristine value. Prompt launch preserves the launcher; in a defined exposed dry sling, a 10-second delayed ignited load should ignite launcher wood in at least 48 of 64 locked seeds. Shielded/fireproof-carrier controls stay viable. |

For all three, capture ignition fronts, active flame/smoke coverage and opacity
over time. Coverage uses smoke opacity of at least 0.2 or a locked perceptible
flame-emission threshold, rather than any nonzero trace amount. A single shot
must not ignite or obscure more than 25% of the viewport
in its first simulated second; later smoke growth must follow admitted fuel,
transport, weather and ventilation. A sustained enclosed blaze may legitimately
fill a room later. Reset, brush editing, chunk seams, offscreen travel, reload,
pause and replay must preserve fuel, water, oxygen, damage and ownership.

Add gas-specific controls: a sealed fire consumes oxygen and extinguishes before
fuel is exhausted; opening a real vent restores oxygen and can reignite hot fuel.
An inert colored-smoke injection dilutes oxygen fraction without inventing oxygen
consumption. Cold or magic-colored smoke keeps its own density/color behavior.

## Ticket roadmap

The dependency-linked table below records the full technical scope. Implementation
now follows the visible milestones above; it is not a requirement to finish the
entire horizontal table before showing a usable system. One implementation agent
works in its isolated worktree.

| Stage | Ticket | Delivery |
| --- | --- | --- |
| Plan | `ctlo-wmy2` | This design, scope and validated dependency roadmap |
| 0 | `ctlo-ck1i` | Lock fixture maps/seeds/profiles, clock adapters and displayed baseline |
| 1 | `ctlo-ldb7` | Reactive, thermal, phase and conductivity material profiles |
| 1 | `ctlo-2q4f` | Versioned sidecars, conservative owner handoffs and persistence |
| 2 | `ctlo-18l4` | Heat exchange, wetting/drying and water/vapor energy |
| 2 | `ctlo-r2ap` | Reference-gated sparse smoke, oxygen and compressible gas transport |
| 3 | `ctlo-8b67` | Seeded hazard ignition, finite-fuel combustion and reaction ledgers |
| 3 | `ctlo-3ndt` | Progressive geometry/structural damage and ash handoff |
| 4 | `ctlo-08fu` | Shared-capacity continuous oil/slime volume beside water |
| 4 | `ctlo-2vcr` | MPM-guided yield-stress sludge without duplicate mass |
| 4 | `ctlo-e4y6` | Contact wetting, melting and adhering surface coatings |
| 4 | `ctlo-iit1` | Future-lightning shock/energy and conductivity interfaces |
| 4 | `ctlo-tk0j` | Substrate oxidation/corrosion, valuables and spent residues |
| 5 | `ctlo-3hb8` | Finite mixed Greek-fire fuel, bound oxidizer and water resistance |
| Later mechanics | `ctlo-x5gv` | True moving rigid-body pivot constraints and save/load |
| 5 | `ctlo-6syy` | Assembled siege launcher, stored work and real projectile release |
| 3-5 | `ctlo-h9of` | Colored flame/smoke and aggregate emissive-buffer integration |
| 1-5 | `ctlo-hjxc` | Optional versioned initial-state prop/export channels |
| 6 | `ctlo-ehqj` | Contained warming forest bonfire and broken-containment controls |
| 6 | `ctlo-ao7l` | Burning-coal siege, thick dry wall and wet-wall control |
| 6 | `ctlo-yvip` | Greek-fire adhesion, corrosion, dousing and delayed-launch risk |
| Final gate | `ctlo-jjye` | All three displayed scenarios under the full-frame budget |
| Deferred, nonblocking | `ctlo-ddvz` | Full conductive lightning-energy propagation |

The baseline precedes runtime work. The pivot is a later visible mechanical feature;
state/heat/gas unlock ordinary fire, while oil rheology/coatings unlock
Greek fire. The three scenario tickets have their own prerequisites and feed the
final acceptance gate. Authoring extends the existing exporter and depends on
completion of the existing all-role admission ticket; programmatic scenarios do
not depend on that authoring path. The deferred electrical propagation ticket is
linked to this epic, but is not a prerequisite for closing the fire milestone.

Original planning validation at aaf24e2: all 24 ticket records were represented here (the epic,
22 required children including this planning ticket, and one independent deferred
electrical follow-up). IDs, acceptance sections, dependency targets and reciprocal
links were checked and `tk dep cycle` reported no cycles. That planning commit
changed no runtime code. The three modular-delivery milestones above were added
by the subsequent user steering; they do not close the broader technical scope.

## Numerical references

The proposed engine choices above are game-model design decisions, not claims of
validated implementations or measured chemistry. The smoke baseline is informed
by [Fedkiw, Stam and Jensen, Visual Simulation of Smoke](https://www.graphics.stanford.edu/papers/smoke/),
which treats motion, buoyancy and moving boundaries. The need to distinguish
thermal/species expansion from zero divergence is illustrated by the primary
[PeleLM model documentation](https://amrex-combustion.github.io/PeleLM/manual/html/Model.html).
The sludge constitutive direction is informed by the primary
[MIT work on Herschel-Bulkley fluids with MPM](https://dspace.mit.edu/entities/publication/e30c9369-d73f-4a81-8802-b4788ac9e097).
These references do not establish our frame budget or replace the required small
reference solvers, conservative ledgers and actual displayed integration tests.
