---
id: ct-ln5z
status: in_progress
deps: []
links: []
created: 2026-11-01T05:12:47Z
type: bug
priority: 1
assignee: kmd
parent: ctlo-d7vm
tags: [smoke, catapult, visual, physics]
---
# Make catapult smoke opacity follow soot rather than transparent exhaust

Flight and impact captures show a dark persistent coal arc. gas_overlay.comp weights FR_GAS_EXHAUST and FR_GAS_SOOT equally in Beer-Lambert optical depth, while the coal profile yields 77% exhaust and only 5% soot. Retain conservative gas transport and pigment, but distinguish transparent combustion gas from soot aerosol in optical depth and update synthetic smoke fixtures to seed the visible species.

## Acceptance Criteria

RED quarter-native flight/impact capture or GPU pixel test demonstrates over-opaque exhaust-only trail. GREEN visual and GPU tests show soot-density-based opacity, controlled dilution along the projectile arc, intact gas species/energy conservation, readable bonfire and battlement flames, paused replay identity, and frame budget. Run catapult demo and full make test.

