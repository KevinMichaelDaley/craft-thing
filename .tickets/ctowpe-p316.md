---
id: ctowpe-p316
status: closed
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

**2026-11-27T20:39:47Z**

Recorded 180 real native frames at 1a94089c on the hidden X11 display, with 16 live owners and 2.000000104 s advanced in every band. The six slowest near/middle/far frames averaged 36.64/37.73/40.40 ms total, 24.66/21.30/23.45 ms in the resident call and 15.64/13.69/17.87 ms in its GPU tick commands. Corresponding middle-12 tick times were 8.72/8.78/9.14 ms. Slow-frame acquisition was 0.01 ms in each band, software presentation 2.84/3.15/2.96 ms, scene drawing 0.18 ms each, and composition 1.18/1.26/1.22 ms. Optimize resident simulation and identify the costly tick phases next; these figures are diagnostics, not uninstrumented acceptance.
