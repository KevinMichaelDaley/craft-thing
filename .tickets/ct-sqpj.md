---
id: ct-sqpj
status: in_progress
deps: []
links: [ct-wgqu]
created: 2026-10-31T02:22:53Z
type: bug
priority: 1
assignee: kmd
parent: ct-wgqu
tags: [fire, combustion, vulkan]
---
# Prevent combustion gas cells from reaching plasma temperatures under connected wood fires

Connected bonfire wood contact paths make all eight dry logs ignite reliably without extra fuel, but gas audit peaks rise to 2331-4299.6 K. Investigate low-mass receiving cells, wood reaction heat partition, airborne soot oxidation and physical heat sinks. Preserve finite fuel and oxygen, combustion energy accounting, wet controls, and existing plume motion.

## Acceptance Criteria

RED GPU tests reproduce hot low-mass gas during simultaneous neighboring wood reactions. GREEN bounds a connected eight-log bonfire to physically plausible gas temperatures across repeated displayed quarter-native runs while retaining 8/8 dry ignition by 60s, 0/8 wet, finite stock/energy accounting, and no plasma-like rendering. No arbitrary unaccounted temperature clamp or extra starter fuel. Full regression passes.


## Notes

**2026-10-31T02:57:52Z**

Initial heat-path audit after quarter-native catapult delivery: shaders/reactive/combustion.comp:318-335 debits soot chemical reserve, sends 70% of primary reaction heat to solid and 30% plus product sensible energy to the selected gas cell. shaders/reactive/flame_gas.glsl:26-35 oxidizes soot in that cell with 4000 kJ/kg reserved energy; both routes can raise temperature sharply when gas capacity is small. This is a hypothesis, not a proven root cause. Next diagnostic should capture species mass, heat capacity, internal energy and position of the hottest cell during the connected eight-log trial, then add a failing energy/temperature test before changing the model. Original bonfire layout remains restored.

**2026-10-31T03:06:00Z**

2026-10-31 displayed quarter-native diagnostic on temporary connected no-extra-fuel layout: run passed dry8/8 wet0/8 by21s, but peak audit3343.7K; another run peaked4602.4K. Full-view GPU readback showed hot cells at wet-side starter edge (e.g. world x361 y92, T2281.6K, mass3.179e-7, O2=2.601e-9, soot6.591e-10, energy5.171e-4). These are thin low-capacity gas pockets. The diagnostic readback matched GPU audit when scanning the whole view. Likely issue is thermal/air mixing or heat coupling around moving/partially occupied cells; neither energy coefficient changes nor audit masking is justified yet. Temporary connected geometry and test diagnostic were restored; original fixture unchanged. Logs: ignored build/bonfire_connected_full_trial.log and build/bonfire_hotspot_global_trial_10s.log.

**2026-10-31T03:10:37Z**

A focused thin-air combustion test with gas preheated to1700K measured2351K after four wood fuel units reacted; starting at293K gave1227K. This does not reproduce the full-scene 4602K pocket or isolate a defect, so the artificial2300K test was reverted rather than imposing a temperature ceiling. Conservative gas transport applies energy flux alongside mass; no direct evidence of stranded transport energy. Next step is capture gas-cell geometry/volume and per-step reaction/thermal flux at the wet-side starter edge around world x361 y92.

**2026-10-31T03:12:07Z**

Checkpoint geometry follow-up on temporary connected layout: hottest wet-side starter cells were (361,92) T2436.1K volume0.0024719 mass1.0344e-7 O2=1.0526e-9 and (362,91) T3591.4K volume0.0051422 mass2.5989e-6 O2=6.2177e-10. At a separate report (362,91) T2662.2K volume0.7024 mass8.3817e-5, showing low density as well as small volume. shaders/gas/geometry_transfer.comp retains gas stock when volume expands and relies on later transport to refill; the rarefied cut-cell heat/air mixing path is the concrete next target. Temporary fixture/diagnostic source restored. Existing conserved transport carries energy with mass; do not apply an arbitrary heat clamp.
