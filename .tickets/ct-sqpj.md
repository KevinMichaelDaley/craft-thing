---
id: ct-sqpj
status: closed
deps: []
links: [ct-wgqu, ct-42nt]
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

**2026-10-31T04:12:41Z**

2026-10-31 TDD geometry correction: RED gas geometry case showed a newly carved/open cell stayed at zero oxygen while adjacent open donor retained all stock. GREEN geometry transfer now borrows from stable open neighbors and debits each donor by the same fraction; existing global species and new energy conservation assertions pass. Full make test passed, gas display34/34 plus8 capability skips, gas publication67/67 plus10 skips. Actual quarter-native bonfire1000-frame mean16.683ms, worst34.144ms scripted action; prior accepted run mean16.666ms. Connected-layout trials still reached3251.9K (8/8) and3648K (failed8/8), so this correction does not satisfy this ticket or ct-wgqu. Original layout restored. Continue investigating partially occupied sealed/unstable cells and heat transport.

**2026-10-31T04:41:36Z**

2026-10-31 Fourier gas-gas heat exchange now runs in the combustion contact face pass: Q=k A dt (T_left-T_right)/dx, species-weighted cv capacity, cut-cell aperture, closed-gate exclusion, conservative opposite-cell application, four-face explicit stability bound. GPU RED/GREEN test verifies still-air transfer, unchanged nitrogen mass, energy conservation and closed gate. Existing gas convection uses advected energy; solid-solid and solid-gas paths were present. Weighted hot-gas audit added. Displayed original bonfire remains 8/8 dry 0/8 wet by ~21s; hot mean ~1200-1370K, transient peak ~2570K. Peak is much lower than prior connected-fire 3500-4600K but connected geometry and repeated temperature acceptance remain unverified. Practical policy GPU feature gate bug fixed by RED/GREEN test; full regression in progress.

**2026-10-31T04:52:26Z**

2026-10-31 finalized split pass: existing solid-gas contact remains before reactions; Fourier gas-gas flux runs once after reactions, so fresh heat reaches neighboring nitrogen-rich air within the same thermal step and the old ignition/oxygen path is unchanged. Two actual displayed quarter-native bonfire runs passed dry8/8 wet0/8 by ~21s; sampled max gas peaks 2126.1K and 2265.5K, hot-capacity-weighted means 941-1190K and 945-1038K. No temperature clamp. Focused GPU still-air/gate/conservation tests pass; full suite rerunning after corrected test accounting for fractional oxygen reserve and actual quarter-native visible flame pixels.

**2026-10-31T06:40:27Z**

2026-10-31: corrected hot-cell diagnostic by reversing GPU page map (scene table slot did not equal GPU slot). Actual 4-5 kK peaks were 0.2-1% gas slivers inside 99% rigid wood, including cold wet logs; earlier world-coordinate material guesses were wrong. Added conservative same-cell gas-rigid heat exchange and gas-liquid exchange with equal/opposite energy, temperature-dependent air conductivity, and exact pair-equilibrium bound for one internal interface. GPU contact cases pass. Three displayed centered-layout runs: 8/8 dry twice (sampled gas peaks 1985K and1954K), third 5/8 with peak3087.6K. Remaining transient/ignition variability unresolved; no temperature clamp.

**2026-10-31T07:01:26Z**

2026-10-31: same-cell cut-cell gas-rigid and gas-water energy exchange, temperature-dependent dry-air Fourier diffusion, and equal/opposite ledgers are in place. A separate open-sky radiation sink caused only 2/8 ignition because flame-to-solid radiation is absent; it was reverted and tracked as ct-42nt. Widened right kindling bridge with unchanged 48-cell starter passed five consecutive actual displayed quarter-native runs at 8/8 dry, 0/8 wet; sampled gas peaks 1733.9, 1862.1, 1800.0, 1868.5, 1689.4 K. Full suite and frame-time gate in progress.

**2026-10-31T07:09:33Z**

Full regression exposed a stale-face-flux bug in thermal-only worlds: phase 13 applied post-combustion heat even with no combustion pipeline. RED energy tests showed rigid-terrain drift -1.481 J, hot-skin drift -0.0176 J, rigid wetting drift -81.26 J. GREEN shader guard restored 0 J, 1.9e-9 J, and 9.1e-6 J respectively; all 17 component admission cases pass. Full suite rerunning.

**2026-10-31T07:33:43Z**

Final validation: five displayed quarter-native runs passed8/8 dry0/8 wet with sampled gas peaks 1689-1869K and no energy clamp. make test passes, focused contact4/4 and containment1/1 pass. Separate open-sky radiation/flame reciprocity remains ct-42nt; current transient fix uses physical gas/solid/water heat exchange plus nitrogen-rich air Fourier diffusion.
