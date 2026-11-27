---
id: ctowpe-jc8w
status: closed
deps: [ctowpe-utq0]
links: []
created: 2026-11-27T19:39:06Z
type: task
priority: 1
assignee: kmd
parent: ctowpe-vtlh
---
# Distribute confirmed replay steps across presented frames within each camera distance band

The 4bbe76ec near trace alternates 30 frames with zero replay and 30 with 32 owner-steps. Middle frames with 32 owner-steps averaged 28.351 ms in the batch. Keep the bounded native elapsed steps and consume due work across regular frames.

## Acceptance Criteria

Show the regular 30 FPS saturated near case uses consecutive frames for the same two elapsed native steps. Verify all due owners, their confirmed clocks, return state, cadence and larger elapsed/catch-up behavior. Measure both production camera resolutions and all distance bands on hidden Xephyr.


## Notes

**2026-11-27T19:47:37Z**

Profiled 60 frames per band on 4bbe76ec. Near: 30 idle vs 30 with 32 owner-steps; those batches averaged 20.905 ms. Middle: 30 idle, 10 with 16, 20 with 32; the 32-step batches averaged 28.351 ms. Far: 35 idle, 5 with 16, 20 with 32; 32-step batches averaged 27.499 ms. Replay uses up to 2 rounds across 16 owners. A host arithmetic check over 180 regular 30 FPS frames retained the per-owner step sequence and maximum lag with one round per frame in all three bands.

**2026-11-27T19:56:38Z**

Regular near steady-state RED failed at the measured shared-submit count. GREEN used one round on each of two consecutive frames, all 16 owners reached the native intermediate and final timestamps, and a 0.125 s foreground increment retained two catch-up rounds. All 17 returned with their original individual water stock and world time. Refactored test compared real fluid cells, face velocities and markers to a serial native reference, passing on :93. The new setup uses the same 120 s allowance already used by the 17-room return; an earlier 25 s setup timed out with synchronization validation. Existing middle cadence and saturation tests passed with validation.

**2026-11-27T19:59:24Z**

The serial-state comparison passes on :93 without validation. With synchronization validation, the 17-room near setup still timed out at the 120 s visibility wait, before the frame assertions. Middle cadence, saturated return and level tick tests passed separately. Investigating setup residency diagnostics instead of increasing the timeout again.

**2026-11-27T20:18:05Z**

The instrumented validation setup progressed from 21 visible slots/0 admitted rooms at 47 s to 58 slots/5 rooms at 142 s. Twelve slots were still loading. The fixture now tracks confirmed admissions and resident slots and applies its timeout only when both stop progressing, while requiring the full ready state on a single tick.

**2026-11-27T20:27:04Z**

The 17-room regular fixture passed the original clock/stock/serial-state assertions with synchronization validation after progress-aware setup. The irregular-frame reference including a moving grain also passed. At 8503946e all six camera means passed and three p95 gates passed; the remaining three are on the parent performance ticket.
