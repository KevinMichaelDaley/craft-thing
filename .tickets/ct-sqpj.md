---
id: ct-sqpj
status: open
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

