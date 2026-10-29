---
id: ct-79vm
status: closed
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


## Notes

**2026-10-29T17:36:17Z**

2026-10-29: Replaced painted far-side wood with seven constructed wood bodies (four deck planks, three posts), each with a valid stone-floor anchor and finite reactive parcels; no duplicate terrain parcel at the old battlement cell. Coal clears the distant wall and ignites/consumes post fuel. Scene 11/11, display 2/2, component admission 13/13, combustion body feedback 6/6. Lit 256x128 on-screen demo completed 205/205 frames. Full make test passes all preceding suites and stops at reactive_profile_gpu_tests because this machine has no Intel hardware compute device.
