---
id: ctowpe-o0i2
status: in_progress
deps: []
links: []
created: 2026-11-27T16:08:21Z
type: task
priority: 1
assignee: kmd
parent: ctowpe-vtlh
---
# Measure and reduce 1080p presentation time on the persistent camera test display

A static frame presented at 1920x1080 on the hidden Xephyr display already exceeds the 33.33 ms camera budget. Measure the supported Vulkan presentation modes, incremental updates, and accelerated Xephyr at the same extent. Keep full-size presentation, physics cadence and the camera acceptance workload intact.

## Acceptance Criteria

Identify the dominant presentation cost with a repeatable measurement and verify any improvement on actual camera frames. Retain correctness for full-frame changes, camera movement and capture.


## Notes

**2026-11-27T16:08:40Z**

Created an independent hidden persistent Xephyr :94 with glamor enabled for an isolated comparison against :93. Both servers advertise Present, MIT-SHM and GLX, and neither advertises DRI3. All acceptance benchmarks remain on the original hidden :93. The comparison uses an identically sized, fully presented 1920x1080 swapchain, with no skip-present option, followed by syscall timing on :93.
