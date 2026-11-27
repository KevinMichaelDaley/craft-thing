---
id: ctowpe-utq0
status: open
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
