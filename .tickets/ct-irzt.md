---
id: ct-irzt
status: open
deps: []
links: []
created: 2026-10-05T02:49:02Z
type: task
priority: 2
parent: dun-vd9a
tags: [vulkan, performance, quarter-native]
---
# Investigate quarter-native Iris Xe performance variability

The dense 640x448 quarter-native benchmark previously passed at 60.82 Hz, but current repeated measurements are 46.49 and 46.07 Hz. A detached build of the prior pushed commit 5a98ec5 measured 45.69 Hz under the same conditions, so the slowdown predates the new broadphase. Both versions report rigid stage 0.000 ms (no active bodies); fluid and granular timings increased. Desktop processes were active during the measurement. Keep the 60 Hz assertion intact and investigate runtime load/power variability before changing solver quality.

## Acceptance Criteria

Repeated baseline/current Iris Xe measurements explain the variance and establish a reproducible 60 Hz quarter-native physics budget without reducing simulation correctness; record CPU/GPU load and power/frequency evidence and retain the strict performance assertion.


## Notes

**2026-10-05T05:13:34Z**

After dun-ci2x, the unchanged strict dense quarter-native gate passed on Iris Xe: 16.101 ms/tick (62.11 Hz), 16.566 ms including rendering (60.37 Hz), fluid 10.334 ms and granular 4.411 ms. Run was serial after correctness/UI checks with Vulkan validation disabled. This fixture has no rigid bodies (rigid stage 0.000 ms); the new contact stage is skipped there. Retaining this ticket because prior runtime variability was not diagnosed by one passing run.

**2026-10-08T06:59:33Z**

After dun-9qub, serial headless dense quarter_native_bench with validation disabled again passed the strict >=60 Hz physics and render gates: 15.660 ms/tick (63.86 Hz), 16.352 ms including rendering (61.15 Hz); rigid 0.000 ms, fluid 9.116 ms, granular 4.572 ms. Retain the variability investigation and do not infer active rigid/fluid performance from this zero-body fixture.

**2026-10-13T06:40:25Z**

2026-10-13 serial comparison after ct-alp7: current dense 640x448/70-slot/16-pressure benchmark 20.274ms physics (49.32Hz), 21.500ms including render; rigid 0, fluid13.993ms, granular4.465ms. Detached pushed baseline232d76b built on same machine/config gives20.240ms physics (49.41Hz),21.299ms including render; rigid0,fluid13.923ms,granular4.465ms. Dense gate remains failing, essentially identical baseline/current, so not caused by pixel contact/raster optimizations. Intel frequency observed1200MHz, max1300,min100 after run; no attribution of variance from this single sample. Strict gate retained.
