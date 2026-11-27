---
id: ctowpe-vtlh
status: in_progress
deps: [ctowpe-0zjo, ctowpe-kudk, ctowpe-o0i2, ctowpe-l2hw, ctowpe-mbc3, ctowpe-8rdr, ctowpe-1kw3, ctowpe-1y1n, ctowpe-04hz, ctowpe-bsh0, cttl-ieib, ctowpe-p316, ctowpe-bvwf]
links: []
created: 2026-11-27T15:00:01Z
type: bug
priority: 1
assignee: kmd
parent: ctowpe-t43r
---
# Bring saturated offscreen camera frames within the 30 FPS budget

On the RTX A2000 12 GB, the saturated half-native production camera exceeds 33.333 ms per presented frame in every distance band. Measure the division between resident physics, offscreen replay, and presentation. Improve the measured limiting work while preserving clock advancement, native physics, stock, the camera cache size, and both viewport sizes.


## Notes

**2026-11-27T15:08:30Z**

A separate half-native timing build on 18162219 sampled the same last 60 frames in each band, excluding warmup. Mean resident tick submissions / offscreen tick submissions / presentation call in milliseconds: near 55.01/12.64/51.85; middle 54.52/11.78/48.63; far 50.59/10.24/50.99. Each frame paid for two separate resident submissions and a zero-step presentation. ctowpe-bfv3 exercises the existing combined presentation path for sealed disk owners. The 720 exact concave gate also exceeds budget.

**2026-11-27T15:27:23Z**

In the 0ae8493e half-native near frame, Vulkan timestamps over the 60 measured frames put the two foreground ticks at 55.174 ms mean, draw at 0.057 ms and image copy/blit at 0.138 ms. Six follow-up native tick captures show the recurring fluid spike at 70.985 and 74.749 ms, while rigid and particle/heat stages stayed below 1.5 and 0.7 ms. Profiling individual phases to identify the expensive solver work.

**2026-11-27T15:58:43Z**

Presentation control on hidden :93 with the half-native 1920x1080 swapchain, 12 warmups and 60 measured calls each: resident chunks rendered with zero physics or replay averaged 64.011 ms (p95 76.802); presenting the existing render buffer averaged 68.555 ms (p95 80.097); two resident steps plus chunks averaged 82.540 ms (p95 101.665). This run exceeded the 33.333 ms budget even with simulation omitted. Xephyr advertises Present and MIT-SHM; xdpyinfo did not list DRI3. Native 720 dry passed at 23.070/23.837 ms mean/p95, dense measured 37.375/40.278 and exact concave 52.725/72.587.

**2026-11-27T16:33:47Z**

Latest profiled native near camera averaged 151.364 ms, p95 216.584. In its 60 measured frames, GPU timestamps placed both resident physics steps together at 67.759 ms mean / 114.878 p95, rendering at 1.188/1.629, and copy/blit at 0.398/0.797. The host vkQueuePresentKHR call, including GPU wait, averaged 134.084 ms. The native 720 gate measured mean/p95 23.238/24.504 dry, 29.560/33.457 dense and 39.143/46.799 concave. Dry passed; dense narrowly missed p95 and concave missed both limits.

**2026-11-27T17:06:16Z**

88238843 production 720 tests on hidden :93: dry mean/p95 23.602/25.631 ms and dense 29.081/32.589 passed. Exact concave 38.079/44.128 still fails. Half-native mean/p95 82.879/107.740 near, 81.336/110.201 middle, 79.536/117.421 far. Each band presented 60 frames with 16 active owners and advanced the world clock by 2.000000104 seconds. Oldest-owner lag was 0.0667/0.0667/0.2667 seconds. Full native pending.

**2026-11-27T17:09:41Z**

Native 88238843 ran the full near/middle/far workload with production dispatch and default presentation. Mean/p95 was 95.165/121.557, 93.551/119.526, and 87.842/114.234 ms. All bands had 16 active owners. World-clock advancement was 2.000000104 seconds per band. The oldest owner lag was 0.0667/0.0667/0.2667 seconds. Allocation priority is verified and closed; the aggregate regression suite is being run on the newer hydrostatic shader revision.

**2026-11-27T17:21:46Z**

Instrumented the same production 1280x720 exact concave workload at 8f26f761 on hidden :93. Over 30 measured frames, the host presentation call averaged 38.405 ms including GPU waiting. GPU rigid work averaged 8.039 ms, fluid 5.315 ms and MPM 2.394 ms; solver work (within rigid) was 6.668 ms. Mean acquire and fence calls were 0.002/0.004 ms. Detailed timestamps add overhead, so acceptance times come from the separate uninstrumented run.

**2026-11-27T17:34:27Z**

8f26f761 production 720 results on hidden :93 (mean/p95, ms): dry 22.783/23.524 and dense 28.743/32.371 pass. Concave 36.417/40.686 misses both thresholds. 1abdfaa8 completed its prebuild; the two production benchmark revisions are serialized before their aggregate suites.

**2026-11-27T19:06:07Z**

At 8abb9206 the saturated 16-owner camera passed every mean with half-native 24.263/24.554/24.440 ms near/middle/far and native 27.627/27.503/27.594. The six p95 values remain above 33.333 ms (half-native 38.107/44.956/44.747, native 41.350/47.327/46.321). The separate 720 dry, dense and exact concave gates passed with p95 4.361, 13.947 and 21.450 ms. Mapped water dispatch and uninterrupted pressure preparation now have separate pending production comparisons under hidden Xephyr.

**2026-11-27T19:20:39Z**

At 782cb32c, with page-indexed projection and mass launches bounded by mapped slot, half-native camera p95 near/middle/far = 36.938/43.107/43.807 ms; native = 44.182/43.728/44.132 ms. All six means and all three separate 720 mean/p95 gates passed. The six saturated camera p95 gates remain open.

**2026-11-27T19:38:05Z**

Pressure reuse at 4bbe76ec passed 7/7 Vulkan pressure checks and all 720 gates. Half-native near/middle/far p95: 36.907/40.843/41.465 ms; native: 38.542/43.320/42.687 ms. Retained 16 confirmed active owners and 2.000000104 s simulated per band. Collecting per-frame actual replay batch counts, wall times and per-kernel device timestamps for the remaining tail.

**2026-11-27T20:18:05Z**

Regular-frame pacing at 28413683: half-native near/middle/far mean/p95 ms = 23.044/33.619, 22.824/28.254, 22.242/29.696. Native = 25.609/32.550, 26.016/33.743, 25.965/35.490. Three of six p95 gates now pass; all six means and all three 720 cases pass. Native-step planning at 8503946e is being measured separately.

**2026-11-27T20:27:04Z**

At 8503946e all 16 owners advanced 2.000000104 seconds per band. Mean/p95 ms near/middle/far: half-native 23.376/35.361, 22.521/30.103, 22.358/28.759; native 26.004/32.696, 25.933/33.940, 26.382/39.327. 720 dry/dense/exact concave p95 4.993/12.417/22.625 ms. Interval correctness now passes for regular and irregular frame input. A rotating-owner half-native trace samples each marker dispatch, pressure and transport, and tracks resident presentation time per frame.

**2026-11-27T20:35:10Z**

Per-frame profiling at 1a94089c on Xephyr :93 measured all six camera cases. In the middle 12 and slowest six native frames by duration, resident presentation (including simulation) averaged 15.309/26.595 ms near, 18.060/23.842 ms middle, and 19.172/25.698 ms far. Corresponding offscreen batch times averaged 8.778/6.792, 7.627/7.889, and 5.864/12.360 ms. Pressure was the largest sampled owner stage on slow frames in all six cases, but the resident call consumed the majority of native slow-frame time in all three bands. The half-native binary measured batch and individual kernels only. ctowpe-p316 instruments the resident contribution directly. These instrumented traces are diagnostic; acceptance timings remain the uninstrumented figures above.
