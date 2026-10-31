---
id: ct-d3oz
status: in_progress
deps: []
links: [ct-0ap7]
created: 2026-10-31T07:31:14Z
type: bug
priority: 2
assignee: kmd
tags: [performance, vulkan, display]
---
# Investigate X11 quarter-native worst-frame spikes

After the heat-equation changes, two X11 1000-frame quarter-native bonfire runs averaged 16.702 and 16.695 ms but had worst frames 43.612 and 39.760 ms at frames 108/109. DC_FRAME_TIMINGS shows input/prepare/paint/title below 0.1 ms and spike inside tick/present; thermal GPU groups remain stable near 3 ms while broad rigid GPU stage intermittently rises from 5.5 to 11–17 ms. tools/bench_bonfire hardcodes unavailable Wayland on this X11 session. Determine whether this is queue scheduling, present mode, or a one-time scene event and make the strict <30 ms worst-frame gate pass on X11 without masking spikes or weakening the gate.

## Acceptance Criteria

Two actual displayed 1000-frame quarter-native X11 bonfire runs exit successfully under the existing <30ms worst-frame gate; preserve autonomous fire and exact heat transport.


## Notes

**2026-10-31T07:47:04Z**

2026-10-31: RED/GREEN fixed benchmark feedback loop: each displayed benchmark frame now advances exactly one 60 Hz world tick, so slow present frames do not enqueue extra rigid steps. RED/GREEN low-latency present mode chooses mailbox if offered, otherwise immediate only with DC_LOW_LATENCY_PRESENT=1; default remains tear-free FIFO. Wrapper detects X11/Wayland and selects low-latency on X11. Two manual X11 immediate 1000-frame runs passed strict gate (mean14.946/worst20.109ms; mean14.932/worst27.694ms). Integrated wrapper pair had one 30.353ms outlier and one24.691ms pass. Gas trace showed no retries and stable acoustic work; high frame38 had 14.9ms tick plus 13.7ms X11 present call. Intermittent X11 present latency remains; keep ticket open until two wrapper repetitions pass reliably without weakening gate.

**2026-10-31T08:04:01Z**

Post-GREEN full make -j8 test passes. Integrated tools/bench_bonfire now auto-selects X11 and opts into DC_LOW_LATENCY_PRESENT=1; two runs measured mean14.859/worst30.353ms (failed by0.353ms) and mean15.031/worst24.691ms (pass). Separate manual immediate runs passed worst20.109 and27.694ms. Low-latency X11 trace at frame38: 14.885ms tick, 13.748ms vkQueuePresentKHR wait, 28.956ms total; gas retry count0. Current surface offers FIFO, FIFO_RELAXED, IMMEDIATE, FIFO_LATEST_READY but no MAILBOX. Default remains FIFO; strict worst gate still flaky from X11 present wait, so ticket remains in_progress.
