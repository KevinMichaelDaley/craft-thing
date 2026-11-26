---
id: ctpa-ptku
status: in_progress
deps: [ctpa-zfxm]
links: []
created: 2026-11-26T11:02:45Z
type: bug
priority: 1
assignee: kmd
parent: ct-xsll
tags: [player, beaver, fire, wetting]
---
# Evaluate beaver surface contact from the visible animated pose

The canonical 40x40 body stores 1,023 parcel owners while the idle artwork displays 422. The visible idle robe has zero canonical texels on a four-neighbor-open rest edge. Thermal and combustion sampling use rigid rest cells even when the visible pose moves or mirrors an owner, so a visible sleeve can miss nearby heat and water.

## Acceptance Criteria

In a real sandbox, local water and heat reach the displayed material region across a walk pose change, reversal, or movement. Reaction and moisture remain attached to the same player and parcel as the surface changes. Occluded parcels retain state, excess mass and heat remain conserved, and neighboring dry controls and the second player stay independent. Verify robe and hat singeing, fur wetting and combustion, and actual framebuffer feedback.


## Notes

**2026-11-26T11:02:54Z**

Source atlas measurements from beaver_regions and the idle frame map: 1,023 canonical owners, 422 visible IDs, and zero visible robe owners with an open cardinal rest neighbor. The GPU renderer projects the pose through its source IDs, while thermal body sampling and combustion currently use rest-space shape queries.
