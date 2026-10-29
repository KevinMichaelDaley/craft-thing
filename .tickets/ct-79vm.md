---
id: ct-79vm
status: open
deps: []
links: []
created: 2026-10-29T16:20:10Z
type: task
priority: 1
assignee: kmd
parent: ctlo-6syy
tags: [fire, physics, scene]
---
# Make the catapult battlements a supported constructed wood assembly

The current demo paints the far-side battlement as reactive terrain cells. The user expects a constructed assembly: named supported wooden parts with structural ownership, retaining the same wall placement and projectile strike.

## Acceptance Criteria

Replace painted battlement geometry with a supported constructed wood assembly. Coal still clears the tall distant wall, strikes the far-side wood, and ignites it at 256x128 game resolution. Burning follows constructed wood parcel, mass, strength, ash, and support rules; no overlap or duplicate mass with terrain cells. Headless and display regressions pass.

