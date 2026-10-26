# Ordinary bonfire runtime model

The current visible milestone is ctfs-81vw. Oil, Greek fire, siege weapons and
combustion of other owners remain separate later work.

The first visual trail was a fixed five-cell column over burning terrain. The
user rejected this on2026-10-26. It is removed. Flame emission must come from a
local thermochemical reaction field carried by the gas solver, never from
neighbor searches, fixed heights, clock noise or visual pigment.

The existing transported soot species is the first airborne carbon/tar fuel
proxy. Each surface reaction reserves4000 energy units per unit of admitted soot
from its primary-heat allowance. Immediate heat plus that chemical reserve equals
the original allowance. Local hot carbon oxidation consumes finite oxygen at
8/3 mass units per carbon mass unit, transfers carbon and oxygen to exhaust,
and releases only the reserved energy. No extra species, transport solver or
runtime CPU field readback is introduced. Nonfuel hot air/exhaust cannot burn;
cooled or oxygen-starved smoke cannot emit a flame. The parameters are locked
before GREEN in `include/dungeoncraft/combustion_parameters.h`:650K onset,
1100K full activity,18/s maximum first-order rate, oxygen mass fraction0.12
extinction and0.21 full activity. These are practical game-model rates, not
a claim of resolved combustion CFD. Rendering exposure is separate from physics.

Gameplay pixels use centimeter-cube thermal geometry: gram-scale cell masses,
J/(g K) capacities and conductivity times face-area/separation of0.01m. The
delivered laboratory heat bench retains its prior unit-conductance calibration.
The persisted bonfire-world tag selects gameplay geometry on restore. The torch
is a finite1500K contact reservoir, not an inexhaustible boundary. Terrain/gas
contact uses bounded pair exchange with a sphere/cell Nusselt approximation,
air viscosity1.8e-5Pa s, Prandtl0.71 and conductivity0.02W/(m K).

The strengthened displayed check removes the torch for five physical seconds.
It exposed the original starter self-extinguishing at496.7K. Gameplay geometry
now keeps that starter burning at1260.7K; water coverage must separately
extinguish it. The earlier single49-cell pour drained into the pit and was not
a valid maintained-contact proof. The revised control verifies actual surface
water while applying a finite sequence of at most360 one-cell pours, in addition
to the initial49-cell pour. This does not assert that a drained small splash
permanently extinguishes a still-hot core.

Delivery remains RED until the real transported flame field, water/oxygen
controls, lateral-flow/roof/vent evidence, containment/bridge checks and displayed
complete-frame measurements pass. Do not treat these notes as completion.
