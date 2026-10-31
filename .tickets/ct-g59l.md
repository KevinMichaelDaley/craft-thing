---
id: ct-g59l
status: open
deps: []
links: []
created: 2026-10-31T02:25:24Z
type: bug
priority: 1
assignee: kmd
parent: ctlo-6syy
tags: [catapult, vulkan, display]
---
# Play the full catapult demo at accepted quarter-native game resolution

The standalone build/catapult_demo still compiles the 256x128 default view and logs it as ready, while the user accepted quarter-native 480x270 for the actual game resolution. Reactive coal and arm parcel slots are hardcoded for a four-chunk row, so simply compiling the demo with quarter-native dimensions would seed the wrong pages.

## Acceptance Criteria

RED displayed regression checks a 480x270 physics/render window, correct coal/wood reactive ownership and full lit non-debug 205-frame playback. GREEN parameterizes scene chunk stride without changing 256x128 scene tests, builds a quarter-native player target, and plays the full demo on this machine with projectile impact and battlement ignition. Run full regression and push.

