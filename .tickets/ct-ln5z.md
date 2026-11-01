---
id: ct-ln5z
status: closed
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


## Notes

**2026-11-01T05:25:28Z**

Catapult flight/impact contact sheet is thinner with 0.05 relative exhaust scattering. Focused exhaust-vs-soot and stone-boundary GPU tests pass. Bonfire remains readable; strict 300-frame worst budget fails in repeated runs (33.487-59.625ms, means about19.3ms). Leave ticket open for performance and full regression.

**2026-11-01T05:51:43Z**

Exhaust optical scattering weighted at 0.05 relative to soot, keeping catapult arc thinner and bonfire visible. GPU exhaust-vs-soot test and paused replay pass; full make test passes. Catapult completed 205 quarter-native frames, wall cleared and battlement ignited. A 300-frame bonfire run passed 16.657ms mean/28.497ms worst; long-run wavelet frame spikes remain tracked under ct-rzrj.
