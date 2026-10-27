# Ordinary bonfire runtime model

The current visible milestone is ctfs-81vw. Oil, Greek fire, siege weapons and
combustion of other owners remain separate later work.

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
onset in a mixed voxel, 1100 K full activity, an 18/s maximum first-order rate,
and oxygen mass fractions of 0.12 for extinction and 0.21 for full activity.
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
