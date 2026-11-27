---
id: ctowpe-utq0
status: in_progress
deps: []
links: []
created: 2026-11-27T19:53:26Z
type: bug
priority: 1
assignee: kmd
parent: ctowpe-jc8w
---
# Preserve deferred fractional replay steps across irregular foreground frame durations

An arithmetic replay of the implementation with alternating 1/60, 1/30, 0.125 and zero-time service calls found a middle-band sequence where the original two-round schedule completed a 0.0083333589 s remainder. The paced path refreshed its target and took a 0.0499999970 s step in its place. Confirmed elapsed time remains monotonic, but the sequence of physical updates can change.

## Acceptance Criteria

Reproduce the elapsed-step divergence with deterministic mixed intervals, including larger catch-up calls and partial pressure/MPM work. Retain the native sequence through deferred rounds while smoothing regular frames. Verify stock, all confirmed epochs and real displayed performance.

## Notes

**2026-11-27T19:54:12Z**

Reduced host reproduction to seven middle-band service calls from world time 2.0 s. Foreground advances: 0.125, 0.125, 2*(float)(1/60), 0.125, 2*(float)(1/60), 0.125, 2*(float)(1/60). On the seventh call the original trajectory visits 2.408333335071802 s, while paced replay skips that confirmed boundary from 2.399999976158142 s. Use the same sequence against actual owner physics.

**2026-11-27T19:59:24Z**

Native RTX reproduction confirmed on hidden :93: the seven real middle-band frames and a zero-time drain leave at least one of the 16 replayed owners at a different physical timestamp from the serial two-round reference. The regression also compares fluid cells, face velocities and complete markers once the timestamp matches.
