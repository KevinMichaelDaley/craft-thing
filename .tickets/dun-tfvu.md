---
id: dun-tfvu
status: closed
deps: []
links: []
created: 2026-09-29T10:05:31Z
type: bug
priority: 0
assignee: kmd
parent: dun-9e5b
tags: [fluid, gpu, rendering]
---
# Keep painted falling water continuous at half-native fluid cadence

Holding the mouse to paint water high above ground produces horizontal bands and empty gaps in the falling stream at the half-native three-tick fluid cadence. Reproduce with a dedicated rendered GPU test, determine whether transport or interpolation causes it, and fix without per-frame readback or losing interactive performance.

## Acceptance Criteria

A continuous high brush produces a visually connected falling stream after repeated frames, with a captured before/after render and a quantitative gap check. GPU mass remains bounded and half-native benchmark stays interactive; all existing fluid/UI tests pass.


## Notes

**2026-09-29T10:14:28Z**

RED regression captures 64x128 before/after 45 held-paint frames; original marker correction produced 8 thin falling-stream rows. Fast downward marker correction reduced to 1/8 transfer; new test reports 1 thin row. Full make test passes. 960x540 GPU benchmark: 85.01 tick Hz, 99.40 tick+render Hz on 64 resident chunks (benchmark does not enable three-tick staging).
