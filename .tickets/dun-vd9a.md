---
id: dun-vd9a
status: in_progress
deps: []
links: [dun-tw2z]
created: 2026-09-27T21:41:36Z
type: task
priority: 0
assignee: kmd
parent: dun-9e5b
---
# Reach 60 Hz Vulkan physics for quarter-native view

## Acceptance Criteria

With 480x270 simulated cells displayed at 1920x1080 and a fully resident 640x448 grid including halo, optimize Vulkan water/grain physics on the local GPU toward 60 Hz without simulation copyback. Profile fluid and granular stages, preserve fluid correctness and world-clock advancement, and validate interactive rendering and streaming.


## Notes

**2026-09-27T22:06:07Z**

After full 608-chunk residency and six-stage fluid update, native 1920x1080 runs 12 presented physics ticks in 1.412 s (8.50 Hz), 76-138 ms frame range on RTX A2000. Continue performance work toward 60 Hz.

**2026-09-27T23:14:01Z**

Native 1920x1080/608 chunks improved from 8.5 to about 39-43 presented ticks/s. GPU average stage ms/tick: rigid 0.9, fluid 9.5, granular 7.2; peak fluid phase ~14 ms after redistribution. 60 Hz target remains open; active sim buffers now device-local and fluid runs at wall-clock speed via variable phase count.

**2026-09-27T23:23:37Z**

Added 960x540 simulated half-native mode with 2x display and 187 resident chunks. Paired 12-frame smoke: half 67.6 ticks/s, 71.1 adaptive frames/s; native 38.9 ticks/s, 39.4 adaptive frames/s. Half 60-frame adaptive sample 62.3 fps. Native 60-frame run exposes marker-count overflow, tracked by dun-mt9o. Native 60 Hz remains open.

**2026-10-05T01:00:54Z**

Continued on Intel Iris Xe (TGL GT2), Mesa 26.0.2 through native Vulkan. Corrected benchmark now maps all 510 viewport chunks at 1920x1080 or 608 simulation chunks at 2048x1216, with 15360/16384 sand grains. Twelve-sample complete-fluid-update runs: 166.95/191.28 ms per physics tick (5.99/5.23 Hz); GPU fluid averages 132.26/151.51 ms and granular 27.64/32.26 ms. Separate timed loops have no simulation copyback; diagnostic stage averages are sampled separately. Fixed partial-edge host velocity overflow in ct-qk58. All 86 tests pass under Vulkan validation. Next: instrument per-fluid-pass timestamps to isolate hydrostatic/pressure/transport/marker costs, then optimize; 60 Hz acceptance remains unmet.

**2026-10-05T01:05:02Z**

User changed active scope: native-resolution physics is no longer required. Focus is now quarter-native: 480x270 physics at 4x display scaling, 640x448 with resident halo (70 chunks), Vulkan on the local Intel Iris Xe. Historical native measurements retained for context.
