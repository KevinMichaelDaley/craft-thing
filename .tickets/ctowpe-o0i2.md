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

**2026-11-27T16:28:32Z**

All static 1920x1080 WSI controls used the RTX A2000, 12 warmups and 60 measured frames. Default FIFO measured 58.270 ms mean/83.653 p95; forced immediate 58.661/74.332; repeated FIFO 58.191/74.151; truthful one-pixel incremental hint on the fixed image 56.329/69.898. The submitting thread consumed 52.3–53.4 ms of CPU time per vkQueuePresentKHR in all four runs. Hidden glamor :94 measured 65.823/76.586, versus 54.498/67.542 in the adjacent ordinary :93 repeat. No mode reached the budget. Keep the original acceptance display and settings. Instruction and I/O sampling is queued to identify what consumes the CPU time.

**2026-11-27T16:33:47Z**

A sampled real 1920x1080 FIFO presentation measured 156 writev calls on the presenting thread, 497667360 bytes in 60 frames, and 3847.618 ms total inside those calls (64.127 ms/frame, including sampling overhead). The largest 4193280-byte segment belonged to a /dev/nvidiactl mapping. The four baseline mode/damage controls averaged 52–53 ms of submit-thread CPU time with no profiler. Testing an identical-byte host staging copy to distinguish transfer cost from socket throughput.
