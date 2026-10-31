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
