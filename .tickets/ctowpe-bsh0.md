---
id: ctowpe-bsh0
status: closed
deps: []
links: []
created: 2026-11-27T18:58:05Z
type: task
priority: 2
assignee: kmd
parent: ctowpe-vtlh
---
# Reuse pressure tile and stencil preparation across consecutive solver phases

When several water phases are recorded consecutively in one tick, the pressure passes rescan tiles and regenerate the aperture stencil at each phase boundary even though there is no intervening edit, body motion or water transport. Check the real replay command stream and reuse eligible preparation while retaining the complete pressure solve.

## Acceptance Criteria

Count native GPU work-list and stencil dispatches for adjacent pressure ranges and for phases separated by a paint, page change or rigid tick. Require byte-exact pressure, velocity, cell stock and marker equivalence against independently prepared phases, including residual pressure on dry tiles. Preserve visible and coupled physics and compare production camera frame times on hidden Xephyr.


## Notes

**2026-11-27T19:03:47Z**

Baseline reproduced four tile scans and four stencil builds per uninterrupted full cycle, with the device comparison failing its preparation count. With the change the full cycle records one of each. On the RTX A2000 with synchronization validation, split native ticks covering phases 0–1, 2–3 and 4–5 recorded 1/1, 1/1 and 0/0 preparations. Pressure field, face velocities, cell stock, full marker records and fluid cycle count matched separately prepared phases after draining residual pressure, painting water and replacing a page. Cached 13-phase replay spanning multiple cycles also matched. Production comparison pending.

**2026-11-27T19:34:37Z**

4bbe76ec finished all seven real Vulkan pressure cases on hidden Xephyr :93 with synchronization validation and no failures. The half-native 16-owner near/middle/far mean/p95 timings were 24.068/36.907, 23.208/40.843, 23.552/41.465 ms. Each band advanced 2.000000104 seconds. Native and 720 comparison running.

**2026-11-27T19:38:05Z**

Native near/middle/far mean/p95 ms: 26.412/38.542, 27.169/43.320, 26.802/42.687. All six camera means pass; p95 remains above the 33.333 ms parent gate. Native 720 dry 3.509/4.464, dense 9.519/12.802 and exact concave 16.991/21.042 all pass. Solver equivalence and both requested production camera measurements are complete.
