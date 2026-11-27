---
id: ctowpe-jc8w
status: in_progress
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
