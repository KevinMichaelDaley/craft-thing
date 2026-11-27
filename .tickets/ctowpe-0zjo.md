---
id: ctowpe-0zjo
status: closed
deps: []
links: []
created: 2026-11-27T15:30:30Z
type: task
priority: 1
assignee: kmd
parent: ctowpe-vtlh
---
# Reduce the downward water-transport spike in the saturated camera workload

Half-native GPU timestamps in a 16-room camera scene isolate the recurring spike to phase 3, transport mode 0. Two downward sweeps took 69.587 and 69.755 ms; horizontal mode 2 took 1.229 and 0.904 ms. Identify expensive work inside the downward sweep and reduce it without altering water, pigments, heat, barriers, clocks, or production cadence. Verify direct GPU equivalence and repeat the same presented-frame benchmark.

## Notes

**2026-11-27T15:34:19Z**

Exclusive native solver diagnostic on :93, half-native production scene. A longer uniform-run scan, cached page lookup, and disabling the dry-line pre-scan yielded downward sweep pairs of 68.034/71.193, 71.497/71.646, and 70.469/70.776 ms. None removed the spike. Kept all three production defaults.

**2026-11-27T15:41:26Z**

A timestamped split by 64-column bands showed 78.685/78.638 ms in downward setup and under 0.4 ms total for all column updates. The compact vertical cache dispatch reduced setup to 15.574/15.333 ms on the same 1088x704 grid with rigid occupancy active. The near diagnostic mean/p95 moved from 134.133/184.979 to 94.141/127.061 ms. All four exact aperture/range cases (soil, holes, partial tiles) and all ten transport/reference cases passed with Vulkan synchronization validation. Full production benchmark comparison pending.

**2026-11-27T15:47:50Z**

The production half-native benchmark on hidden Xephyr :93 passed the simulation-time checks with 16 owners and two 60 Hz steps for each of 60 measured frames per band. Near/middle/far mean and p95 in ms: 97.241/126.275, 94.972/132.820, 98.886/134.871. The corresponding baseline pairs were 143.243/195.334, 135.919/191.281, 133.318/195.229. Both graphical regressions for heated-water return and saturated room-clock return passed with Vulkan synchronization validation.

**2026-11-27T15:54:40Z**

The production 1920x1080 repeat on Xephyr :93 completed all three distance bands. At 608 resident slots, 16 owners, 12 warmups and 60 samples per band, near/middle/far mean and p95 in ms were 173.744/218.655, 167.248/212.237 and 175.243/219.521. Corresponding baseline pairs were 390.953/964.926, 417.056/1103.494 and 420.001/1014.992. Each band advanced the intended 2.000000104 seconds. This closes the measured downward-sweep spike reduction. Parent ctowpe-vtlh retains the unmet 33.333 ms presented-frame gate.
