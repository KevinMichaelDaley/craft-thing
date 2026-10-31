---
id: ct-5gg7
status: closed
deps: []
links: []
created: 2026-10-31T21:50:41Z
type: task
priority: 1
assignee: kmd
parent: ctfs-81vw
tags: [fire, smoke, display]
---
# Set transported bonfire smoke near 75 percent display alpha

User requests smoke about three-quarter alpha. Tune the existing Beer-Lambert display coefficient against representative soot density without changing gas state or adding a fake screen animation.

## Acceptance Criteria

GPU display regression measures 0.70-0.80 alpha for a representative transported soot cell; fullscreen quarter-native bonfire remains readable and passes dry/wet ignition checks; full make test passes.


## Notes

**2026-10-31T22:10:44Z**

GPU RED measured 0.959 composite alpha at opacity 2000; GREEN opacity 832 measured 0.731 for 2e-6 soot in 1.2e-3 gas, with 16 red levels of contrast. Direct fullscreen X11 recording build/ct_5gg7_plume.mp4 shows a readable moving smoke plume; 21.7 s displayed bonfire passed 8/8 dry and 0/8 wet. Full tools/with-vulkan make -j8 test passed exit 0.
