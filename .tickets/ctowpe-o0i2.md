---
id: ctowpe-o0i2
status: in_progress
deps: [ctowpe-ysgp]
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

**2026-11-27T16:36:52Z**

In an isolated diagnostic retaining the real Vulkan swapchain and identical pixels, the unmodified X11 transfer measured 54.410 ms mean and 65.572 p95. A normal memcpy into host storage worsened the result (227.023/382.915). Streaming loads from the NVIDIA mapping into host storage before forwarding the same iovecs reduced it to 16.916/31.907 ms. Warmup memcmp verified 26,697,596 staged bytes for the streaming run, including repeated transfers after partial writes. This remains an experimental measurement. Actual populated half-native camera measurements are queued; the production presenter is unchanged.

**2026-11-27T16:52:22Z**

Hidden Xephyr :93, populated 960x540 internal render with real 1920x1080 swapchain and experimental streaming-read staging. Near mean/p95 58.172/92.748 ms, middle 52.997/95.085, far 49.405/86.017. All 16 active owners advanced 2.000000104 s. Warmup compared 8,394,780 bytes. This diagnostic improves presentation but misses the 33.333 ms gate. No interposition shipped.

**2026-11-27T17:34:27Z**

Isolated two-device diagnostic keeps RTX physics, rendering, blitting and composition. It reads the complete composed image into cached coherent RAM and uploads it to a same-format full-size FIFO Vulkan swapchain on llvmpipe, in the same Xephyr window. Initial half-native frame 10 matched all 8,294,400 staged bytes and all 1,228,800 visible RGB pixels. Near/middle/far mean/p95 30.961/50.886, 29.913/58.565, 28.376/56.352 ms. The first validation run flagged an acquire-to-layout-transition stage hazard in the experimental presenter. A corrected diagnostic is queued and the runtime presenter has not changed.

**2026-11-27T17:40:56Z**

Same-format 1920x1080 BGRA8_UNORM FIFO transfer control with a different full-frame pattern every frame: RTX WSI mean/p95 56.643/68.968 ms and llvmpipe WSI 1.424/1.805 ms. ctowpe-ysgp tracks production integration of the verified two-device experiment. Corrected 720 concave passes the frame budget with byte-exact readback; full native and half-native diagnostic verification is queued.
