---
id: ctss-cf7j
status: open
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
