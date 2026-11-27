---
id: ctowpe-p316
status: in_progress
deps: []
links: []
created: 2026-11-27T20:33:35Z
type: task
priority: 1
assignee: kmd
parent: ctowpe-vtlh
---
# Measure resident GPU simulation and X11 transfer on the slowest saturated camera frames

After the replay scheduling fix, resident presentation accounts for 26.595 ms of the six slowest native near frames on average. Attribute this interval before changing water kernels or the display path.

## Acceptance Criteria

Measure resident native ticks, scene drawing and composition, acquisition and software presentation separately on all three camera bands. Verify 16 live owners and two seconds of simulated world time per band and identify the next cost to reduce.


## Notes

**2026-11-27T20:34:57Z**

At 1a94089c the near and middle native traces retained 16 owner steps per frame. Median-center vs slowest-six mean resident times were 15.309 vs 26.595 ms near and 18.060 vs 23.842 ms middle. Matching offscreen batch times were 8.778 vs 6.792 ms near and 7.627 vs 7.889 ms middle. The hidden-display native follow-up records separate resident device timestamps for ticks, drawing and composition, plus host elapsed acquisition and software presentation times.
