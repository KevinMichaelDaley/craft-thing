---
id: dun-tfvu
status: in_progress
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

