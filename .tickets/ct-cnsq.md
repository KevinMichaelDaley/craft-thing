---
id: ct-cnsq
status: closed
deps: []
links: []
created: 2026-10-11T02:19:08Z
type: bug
priority: 1
parent: dun-mi1e
tags: [gpu, rigid, render]
---
# Stop settled rigid-body edge shimmer

Concave rigid bodies continue shimmering after they appear settled. Reproduce late-frame pose and occupancy changes, stabilize supported rest without preventing support removal or moving-body wake-up, and validate the paced Vulkan visual scene.


## Notes

**2026-10-11T02:33:42Z**

RED reproduced changing settled world poses and differing rendered frames. GPU rest stabilization now requires upward support and 30 quiet ticks with bounded instantaneous and accumulated displacement; physics still solves each tick. Motion updates, moving supports and MPM contacts inhibit/reset rest. Camera rebases translate the transient anchor. Seven compound regressions pass, including exact settled poses, stationary concave passenger, slow conveyor, support removal, rebasing and V4 persistence. Final suite/visual verification underway.

**2026-10-11T02:36:04Z**

Verification complete: make test passed all 143 tests with zero Vulkan validation errors; quarter-native fluid solver 29/29, quarter-native rigid solver 13/13, and controls 5/5 pass. Earlier paced Vulkan run passed 120 consecutive byte-identical settled frames with zero velocities. Final window retries encountered the independently tracked ct-d50u frame-callback stall (including an unlocked, powered-on desktop); no visual assertions weakened. Latest stationary-platform/conveyor/rebase regressions all pass.
