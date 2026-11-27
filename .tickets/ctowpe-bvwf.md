---
id: ctowpe-bvwf
status: closed
deps: []
links: []
created: 2026-11-27T20:39:47Z
type: task
priority: 1
assignee: kmd
parent: ctowpe-vtlh
---
# Reduce resident simulation time on saturated native camera frames

Resident GPU ticks take 13.69 to 17.87 ms in the slowest native frames. Attribute those ticks to physical phases and optimize the dominant cost without changing simulated state or pacing.

## Acceptance Criteria

Measure rigid, particle, bulk and six scheduled water phases on the native workload. Demonstrate preserved solver results and repeat uninstrumented native, half-native and 720p benchmarks on Xephyr.


## Notes

**2026-11-27T20:47:57Z**

Across 180 native frames at 469573d1, phase 5 averaged 11.500/9.649/10.493 ms in the slowest six near/middle/far frames. Rigid work averaged 3.562/3.429/2.971 ms. Phase 5 accounted for 12.169/12.569/13.265 ms in the six GPU-heaviest resident ticks. Added RED resident and seam command tests that both recorded twice the needed pair invocations. The one-writer shader passed both. Horizontal and vertical seam comparisons with the monolithic solver passed both with marker correction enabled and disabled under synchronization validation. The independent shader comparison is being repeated with explicit device-to-staging cell readback.

**2026-11-27T20:51:43Z**

After explicit device upload and readback, the old and new GPU kernels changed 1,414 to 2,470 cell masses and heat values per comparison. Both parities and both vertical-only and full correction sequences matched byte-for-byte for mass, pigment, heat, and velocity. The persistent serial-reference regression independently checked every resident cell after all four sweeps, required nonzero transfers in each sweep and across the page boundary, and passed under Vulkan synchronization validation for both parities. All six enabled/disabled seam checks passed. Uninstrumented half-native near/middle/far p95 measured 29.670/32.335/28.390 ms, with 16 owners and 2.000000104 s advanced in every band.

**2026-11-27T20:56:10Z**

Uninstrumented 1920x1080 native near/middle/far mean/p95 in ms: 23.238/29.832, 22.927/30.157, 22.349/28.465. Half-native: 22.323/29.670, 22.269/32.335, 22.503/28.390. Every band completed 60 frames, 16 active owners and 2.000000104 simulated seconds. The 720p dry, dense and exact concave runs also returned success with p95 3.816/12.978/22.973 ms. All nine production performance cases pass on persistent hidden Xephyr :93.
