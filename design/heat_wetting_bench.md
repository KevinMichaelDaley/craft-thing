# Interactive heat and wetting bench

Active visible milestone: `ctfs-2hu1`. Deliver this module before smoke or burning.
The scene uses matching stone/wood contact benches, one dry and one receiving
water. Temperature and wetness have separate overlays, independent of authored
color. A finite contact probe lets the player move heat into nearby matter.

The single shared C/GLSL parameter source is
`include/dungeoncraft/thermal_parameters.h`. Ambient is 293.15 K. The probe starts
at 350 K with heat capacity 200, holding 11,370 energy units above ambient.
The initial hot stone is also 350 K. Heat exchange consumes finite reservoir
energy; temperatures are not clamped. By the diffusion maximum principle, these
sources keep this first module below the existing 373.15 K boiling threshold.
Steam, gas convection and ignition remain later visible features rather than
being cosmetically represented here.

Thermal face flux uses harmonic conductivity and the accepted physical interval,
with a four-neighbor monotonicity bound for large time steps. Water uptake is at
most 0.06 mass units per second across all four faces, bounded by available liquid
and the substrate's material-derived capacity. The same Q16 mass and proportional
enthalpy leave liquid and enter bound water. Dry reference mass is metadata, not
another heat inventory. Pause and rendering alone do not advance either process.

RED reference checks lock material-derived wet heat capacity, signed conductive
flux, four-face stability, finite probe energy, wetting mass/enthalpy conservation,
preserved parcel/hazard history and transactional invalid-input/pause behavior.
The runtime module must additionally pass GPU parity, real display interaction,
capture, whole-frame measurements and the edits/persistence it exposes. A passing
CPU reference alone does not complete the visible milestone.
