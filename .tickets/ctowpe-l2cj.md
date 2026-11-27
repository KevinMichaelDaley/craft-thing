---
id: ctowpe-l2cj
status: closed
deps: []
links: []
created: 2026-11-27T14:32:24Z
type: task
priority: 2
assignee: kmd
parent: ctowpe-t43r
tags: [offscreen, performance, vulkan]
---
# Measure presented frame costs with a saturated offscreen camera cache

## Design

Measure the production camera path after a two-screen eviction at half-native 960x540 and native 1920x1080. Separate setup, admission and warmup from steady presented-frame samples. Validate actual offscreen owner count and elapsed time, capture mean, p95 and worst frame, and run the existing native performance gate on the same hidden display without concurrent test GPU loads.

## Acceptance Criteria

Record both measurements, GPU/viewport/slot counts, sample counts and presentation settings. Keep any missed native frame budget open with a reproducible measurement.

## Notes

**2026-11-27T15:00:09Z**

Exclusive saturated baseline on Xephyr :93, NVIDIA RTX A2000 12 GB, DC_LOW_LATENCY_PRESENT=0. Half-native 960x540, 1088x704 atlas, 1920x1080 swapchain, 187 resident slots. Initialization took 11.469 s. Each band retained 16 owners for 60 measured frames after 12 warmup frames and advanced 2.000000104 s. Near setup/warmup 10.735 s and mean/p95/max 138.844/181.421/190.873 ms. Middle 20.900 s and 141.390/197.088/230.824 ms. Far 22.858 s and 134.310/183.163/232.963 ms. Budget is 33.333 ms mean and p95. ctowpe-vtlh tracks the misses.

**2026-11-27T15:05:26Z**

Exclusive native 1920x1080, 2048x1216 atlas, 1920x1080 swapchain and 608 resident slots. Same RTX A2000, Xephyr :93, low-latency setting, cadence and sample count as half-native. Initial setup 42.269 s. Each band retained 16 owners and advanced 2.000000104 s. Near setup/warmup 37.124 s and mean/p95/max 238.327/433.023/447.323 ms. Middle 94.880 s and 234.573/469.834/497.823 ms. Far 141.290 s and 213.736/420.800/511.045 ms. Both standalone benchmarks exited 1 for the frame budget after passing the ownership and clock assertions.

**2026-11-27T15:08:30Z**

The existing 1280x720 gate was also run without a concurrent test GPU workload on :93. Dry mean/p95/max 22.656/24.860/25.024 ms and dense water/grains 27.894/31.595/32.099 ms passed. Exact concave bodies at 65.294/131.048/132.518 ms failed. Each used 30 frames, two fixed 60 Hz steps, cadence 6 and 270 slots. The measured saturated production frame budget remains open under ctowpe-vtlh.
