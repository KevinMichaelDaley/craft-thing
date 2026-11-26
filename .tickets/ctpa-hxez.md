---
id: ctpa-hxez
status: closed
deps: []
links: []
created: 2026-11-26T11:11:14Z
type: bug
priority: 2
assignee: kmd
parent: ctfs-dmxv
tags: [fire, smoke, tests]
---
# Verify conservative gas transfer through the smoke-box roof carve

The optional displayed smoke-box test and frame diagnostic link omitted the player implementation used by the level. Restoring linkage exposed a stale roof-vent expectation: it requires zero oxygen in the opened cell, while the conservative geometry remap fills connected new voids from neighboring gas stock.

## Acceptance Criteria

Build and run the displayed smoke-box regression from the normal linked level. Establish that the brush opens the roof, gas species remain conserved across the immediate edit, the new open cell gets oxygen from the existing atmosphere, and exhaust subsequently exits above the opening.


## Notes

**2026-11-26T11:11:28Z**

The original display test and frame diagnostic fail to link with undefined dc_players_* symbols. Both compile and link with the normal level objects. The recovered display battery reaches 4/5 on the RTX A2000 and fails the zero-oxygen assertion in the carved cell. ct-pb6c already documents conservative fill of connected new voids.

**2026-11-26T11:12:21Z**

Pinned main b800354f reproduces the same roof-vent oxygen assertion when linked against the existing level and player objects. The prior incorrect zero expectation fails before plume measurement.

**2026-11-26T11:21:38Z**

The default suite now runs the supported displayed smoke profile. Its roof case verifies the original closed cell, full open void, oxygen arriving from neighboring stock, all five gas species conserved across the carve, and measurable exhaust above the opening. Final Vulkan run passed 5/5, including 0.00897663813 escaped exhaust after 120 simulated ticks. Default-recipe checks passed 2/2 and GPU dependency lint 9/9. The separate implicit pressure clock failure reproduces before and after the sandbox change and is tracked as ctpa-mv8b.
