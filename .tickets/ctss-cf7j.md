---
id: ctss-cf7j
status: closed
deps: []
links: []
created: 2026-11-26T14:00:34Z
type: bug
priority: 1
assignee: kmd
parent: ctlo-yvip
tags: [fire, corrosion, vulkan]
---
# Preserve finite coating stock under sustained siege corrosion contact

On 69ba82e4, the standing pool keeps the ignited Greek load next to the gold and ingredient samples. The Vulkan --corrode case reports a coating oxidizer value of 2195505920 Q16 and no longer finds the gold record at its sampled cell after 30 s. At 55d2438d the existing moving-load corrosion fixture passes, with gold progress 0.0016 to 0.0269 and identical progress after reload. The sustained-contact path needs its own regression; merely extending the moving fixture does not establish sample and reagent conservation.

## Design

Reproduce persistent contact with actual authored sample and donor owners. Follow parcel identity through any material conversion or movement, and account for transferred coating and spent oxidizer alongside finite reaction products. Check ordering and ownership at the contact and reaction passes.

## Acceptance Criteria

A locked Vulkan persistent-contact replay keeps sample and donor state finite and accounts for all coating and oxidizer. Gold and ingredient corrosion advance through actual film transfer and persist through reload. Existing moving-load corrosion, submerged dousing and conservation checks pass.

## Notes

**2026-11-26T14:24:49Z**

The authored 64-cell RED replay fails on d5f030db with invalid reactive energy/exposure. The new proposal and transfer phases allow one terrain invocation to write each recipient and a donor to be debited by at most that one invocation after combustion. Whole Q16 oil, jelly and bound oxidizer move together with the donor sensible heat. No spent flag is introduced by deposition. Twelve focused steps close the parcel/product mass balance to .01 Q16 and heat to 4.77e-7 kJ. Gold and ingredient progressed to .042435 and .050583, and the donor plus both recipients restored byte for byte from the full checkpoint. The native 30 s pool replay advanced clean gold to .009047 and clean ingredient to .565406 while retaining valid owners. Moving-load corrosion and matched dousing remained green, including the unchanged 45080/0 Q16 late-window result. Deferred reciprocal carrier transfers are tracked separately in ctsc-jisc.

**2026-11-26T14:26:38Z**

The separate pre-coated residue diagnostic failed its first partial-gold assertion on both d5f030db and the sustained-contact implementation. The verified baseline is tracked in ctsc-plvx. The clean contact replay and the moving corrosion integration both pass.
