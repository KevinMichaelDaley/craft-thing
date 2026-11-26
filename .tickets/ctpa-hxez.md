---
id: ctpa-hxez
status: in_progress
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
