# Ordinary bonfire runtime model

The current visible milestone is ctfs-81vw. Oil, Greek fire, siege weapons and
combustion of other owners remain separate later work.

## Current user override: constructed pit and rigid fuel

The terrain-only fixture below is historical and rejected. Do not launch it as
the requested bonfire. Current scope requires constructed structural stone
members anchored to ordinary ground, with independent rigid logs and kindling
stacked naturally inside the pit. The proposed small stack uses four short
kindling sticks, one finite hot ember stick, two lower logs and a bridging upper
log. The main logs begin cold/unlit and settle through real contacts and gravity.
A matching saturated stack provides the wet control. No wooden body is pinned.

ctfs-mx1x supplies only necessary BODY-owner thermal/contact/gas exchange,
combustion, burned-rest-pixel collision/mass updates, residue/fracture and
persistence adapters. Existing reactive rest handles and owner handoffs remain
authoritative; CPU runtime ownership lists and fake terrain under rigid visuals
are prohibited. The old fixture produced a validated-structure-snapshot rejection
on exit; this supported save path must be diagnosed and fixed before delivery.

Presentation is fullscreen, magnified without changing the simulation/render
grid or brush coordinates. It shows physical elapsed time, ignition progress,
inspection and pause/step/reset/exit controls. New automated diagnosis has a
30-second final wall bound with3-second calibration and10-second diagnostic modes and one-second progress output. Timeout is failure; no
multi-minute opaque native run is accepted. Any accelerated test must preserve
the same physical steps and does not replace normal-clock visual validation.

## Historical terrain prototype evidence

The first visual trail was a fixed five-cell column over burning terrain. The
user rejected this on 2026-10-26. It is removed. Flame emission must come from a
local thermochemical reaction field carried by the gas solver, never from
neighbor searches, fixed heights, clock noise or visual pigment.

The existing transported soot species is the first airborne carbon/tar fuel
proxy. Each surface reaction reserves 4000 energy units per unit of admitted soot
from its primary-heat allowance. Immediate heat plus that chemical reserve equals
the original allowance. Local hot carbon oxidation consumes finite oxygen at
8/3 mass units per carbon mass unit, transfers carbon and oxygen to exhaust,
and releases only the reserved energy. No extra species, transport solver or
runtime CPU field readback is introduced. Nonfuel hot air/exhaust cannot burn;
cooled or oxygen-starved smoke cannot emit a flame. The parameters are locked
before GREEN in `include/dungeoncraft/combustion_parameters.h`: 500 K effective
onset in a mixed voxel, 1100 K thermal full activity, an18/s reference first-order rate,
and mass-action oxygen scaling normalized at0.21. Ordinary reaction has no
oxygen extinction or ignition cutoff; finite stoichiometric oxygen is consumed
continuously, and actual reaction tends to zero with available oxygen. The hot
ignition-memory flag is not a flame source: only admitted finite reaction power
produces visible flame. Cooling or wetting can clear thermal ignition memory.
These are practical game-model rates, not
a claim of resolved combustion CFD. Rendering exposure is separate from physics.
The initial 650 K bulk gate produced no flame in the actual bonfire: the 1254 K
fuel surface mixed with air into 547.7 K gas cells. The 500 K value represents fresh
organic tar's unresolved hot reaction front in those mixed cells; it is not a
claim that pure solid carbon autoignites at 500 K. Both 300 K and 480 K cooled-carbon
controls remain nonreactive, and equally hot nonfuel gas remains nonemissive.
The initial display exposure of 80/W left the now-active starter below the
8-bit display's useful range. The chosen exposure is 200/W, applied only to
local reaction power. It changes brightness, not fuel, heat, transport or extent
of the reacting gas. Flame color is stylized emission, not a bulk-temperature
blackbody measurement.

The display reads a device-local Eulerian reaction-power channel written when
surface or airborne transactions actually commit. Primary wood oxidation is
included: it releases several watts at the fuel/air interface, while the first
starter's secondary gas reaction measured only 1.4 mW at 13% oxygen. Computing
only a future gas-reaction rate omitted that real primary source. The channel
stores admitted power, reaction temperature, slot generation and source kind;
it is derived output, not another inventory or an advected flame particle.
Gas fuel, oxygen, heat and momentum continue using the existing transport.
Slot generations and cold-import clearing prevent stale emissions on streaming.
Zero physical time preserves the channel. Its cache is rebuilt by subsequent
physical ticks after restore; chemical and authored state remain persistent.

Each solid chooses its oxygen recipient from the best available exposed supply,
in a separate existing lifecycle pass before any reaction writes gas. The frozen
choice prevents two gas invocations racing to mutate one parcel. An earlier
first-neighbor rule incorrectly preferred starved upper air over fresh side air;
the permanent side-air test rejects that behavior. The sealed starvation fixture
now genuinely seals the other three wood faces with stone. Heat transfer is
independent of that chemistry owner: every gas/solid face has a bounded paired
flux, computed then gathered once by each endpoint without float atomic races.

Ordinary reactions preserve gas momentum and add or conserve local gas mass.
The practical policy's prior advective speed bound therefore remains conservative;
it can retain its warm transport route while geometry still repairs ash openings.
Explicit acoustic policies refresh their temperature-dependent wave-speed bound.
The original unconditional invalidation measured 40.956–44.532 ms mean and
49.258–85.320 ms worst. After this fix, four actual displayed 1000-frame runs
measured 21.354/24.152, 21.242/24.309, 21.280/23.743 and 21.242/23.880 ms
mean/worst. These precede the final automatic-start fixture and are not claimed
as its final mature-fire performance evidence.

The authored automatic-start fixture uses cold, unlit main logs and thin wood
kindling, four ambient-temperature dry-leaf tinder cells, and one connected
six-voxel hot charcoal starter. Its approximately 2.1 g of charcoal carries about
2940 J initial sensible heat and finite profile fuel; no source is replenished.
Air channels and contact splints connect the tinder bundles to the logs. A CPU fixture
audit requires tinder air faces connected to the sky and direct wood contact.
The saturated control wets both wood and leaf tinder. Charcoal is the initial
ignition source, not a flag painted onto every log. The displayed long test tracks
every intended wood pixel, records temperatures/ignition, and uses the existing
air brush to remove only the six starter cells after four wood cells establish
flame. Continued combustion after that removal remains a required acceptance
check. Automatic whole-pile propagation is still RED at this checkpoint.

Gas oxygen and exhaust amounts are continuous even though solid fuel uses Q16.
Rounding each small oxygen debit upward had charged 2 quanta for a wood reaction
requiring 1.2, an approximately 67% over-debit during one-quantum frames. It also
overcharged charcoal's 1.5 ratio. The corrected reference and GPU transaction
retain the material ratio and still reserve available oxygen before committing.
This removes a substantial artificial starvation source; it is not an exact
per-frame convergence requirement.

Gameplay pixels use centimeter-cube thermal geometry: gram-scale cell masses,
J/(g K) capacities and conductivity times face-area/separation of 0.01 m. The
delivered laboratory heat bench retains its prior unit-conductance calibration.
The persisted bonfire-world tag selects gameplay geometry on restore. The torch
is a finite 1500 K contact reservoir, not an inexhaustible boundary. Terrain/gas
contact uses bounded pair exchange with a sphere/cell Nusselt approximation,
air viscosity of 1.8e-5 Pa s, Prandtl number of 0.71 and conductivity of 0.02 W/(m K).

The strengthened displayed check removes the torch for five physical seconds.
It exposed the original starter self-extinguishing at 496.7 K. Gameplay geometry
now keeps that starter burning at 1260.7 K; water coverage must separately
extinguish it. The earlier single pour of 49 cells drained into the pit and was not
a valid maintained-contact proof. The revised control verifies actual surface
water while applying a finite sequence of at most 360 one-cell pours, in addition
to the initial pour of 49 cells. This does not assert that a drained small splash
permanently extinguishes a still-hot core.

Delivery remains RED until the real transported flame field, water/oxygen
controls, lateral-flow/roof/vent evidence, containment/bridge checks and displayed
complete-frame measurements pass. Do not treat these notes as completion.
# Final autonomous ignition gate

Every one of the three intended dry main rigid logs must consume its own fuel
in the normal autonomous displayed run. Track authored log identity through
immutable parcel origins, including owner changes. Ember or kindling combustion
and nearby flame pixels do not count. The HUD reports N/3 and the permanent
bounded test requires 3/3; logs may ignite at different times. Main logs start
cold and unlit, with a finite starter ember and naturally arranged rigid
kindling. The saturated control remains separate. No scripted log ignition,
forced per-log burning flags, infinite heaters or cosmetic flame masks qualify.
