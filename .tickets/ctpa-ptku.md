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

**2026-11-26T11:48:43Z**

Wetting substage. RED 29c805d6 reproduced a dry owner when water touched the displaced fur texel in a standalone Vulkan fixture and when water touched the visible robe in a live player window. GREEN c46ddff2 resolves the active frame and current body generation, projects each visible texel contact quad, and transfers water and enthalpy to the original parcel. The final fixtures verify exact water debits, mirrored ownership, state retention through a hidden frame, canonical contact after detaching the sprite, and darker robe pixels at the reversed walk pose. Body checks 2/2, live player 5/5, sprite 1/1, existing thermal-water 4/4, player CPU and shader validation pass. Heat transfer and atmospheric combustion still need pose-aware contacts and live validation.

**2026-11-26T11:50:58Z**

The combined sandbox regression passed 3/3 on the wetting revision, including the authored clearing, legacy gasless save migration, and exact heated fur and gas checkpoint reload. The strict main suite stopped at the independent split desktop capture check under ct-d7q8.

**2026-11-26T12:22:12Z**

Heat-transfer substage. RED 46269073 reproduced zero heat transfer to the displaced fur and a missed probe on the live walking robe. GREEN 171c4e02 samples and deposits through the same projected parcel contacts. The final Vulkan runs pass 3/3 body and 6/6 live player cases. The source debit and owner gain matched at 33.713882 with the normal pose and within 0.000001 at 32.846755 after mirroring to the hat. The robe warms at its displayed pixel while the second player retains its starting energy. The thermal display, water, sprite, and sandbox suites passed at GREEN alongside player/reactive CPU tests. The refactor build and SPIR-V validation pass. Atmospheric oxygen selection, reaction placement, and live singeing across poses remain open. The strict full run for this branch is queued behind the two previous pinned revisions.
