---
id: ctowpe-0zjo
status: in_progress
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
