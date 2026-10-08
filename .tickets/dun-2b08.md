---
id: dun-2b08
status: open
deps: [dun-x9ei, dun-c459, dun-2nbs, dun-lszt]
links: [dun-8xhl, dun-tw2z, dun-z337]
created: 2026-09-26T05:25:21Z
type: task
priority: 2
assignee: kmd
parent: dun-v2vu
tags: [gpu, benchmark]
---
# Benchmark active cells at native display resolutions

Measure 1024x576, 1920x1080, and 3840x2160 active viewports with rigid/fluid/sand scenes and cold camera movement.

## Acceptance Criteria

Report per-stage GPU times, CPU frame/streaming times, peak GPU residency, frame pacing, and whether each resolution sustains 60 Hz on the tested device.


## Notes

**2026-10-08T06:54:15Z**

User selected quarter-native physics; prioritize 640x448 simulation (480x270 visible, 1920x1080 display) on this Intel Iris Xe Vulkan machine. Include active XPBD stacks and moving supports together with dense fluid/MPM, plus cold startup and camera movement. Existing quarter_native_bench has no rigid bodies, so its zero rigid-stage cost is insufficient to establish the combined active-body budget. A temporary headless three-body empty-fluid quarter-native profile during dun-9qub measured approximately 1.98 ms warm rigid GPU time and 11.37 ms for the full GPU tick; this is not the dense coupled-scene gate.
