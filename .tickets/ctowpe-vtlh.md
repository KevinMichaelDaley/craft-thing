---
id: ctowpe-vtlh
status: in_progress
deps: []
links: []
created: 2026-11-27T15:00:01Z
type: bug
priority: 1
assignee: kmd
parent: ctowpe-t43r
---
# Bring saturated offscreen camera frames within the 30 FPS budget

On the RTX A2000 12 GB, the saturated half-native production camera exceeds 33.333 ms per presented frame in every distance band. Measure the division between resident physics, offscreen replay, and presentation. Improve the measured limiting work while preserving clock advancement, native physics, stock, the camera cache size, and both viewport sizes.


## Notes

**2026-11-27T15:08:30Z**

A separate half-native timing build on 18162219 sampled the same last 60 frames in each band, excluding warmup. Mean resident tick submissions / offscreen tick submissions / presentation call in milliseconds: near 55.01/12.64/51.85; middle 54.52/11.78/48.63; far 50.59/10.24/50.99. Each frame paid for two separate resident submissions and a zero-step presentation. ctowpe-bfv3 exercises the existing combined presentation path for sealed disk owners. The 720 exact concave gate also exceeds budget.

**2026-11-27T15:27:23Z**

In the 0ae8493e half-native near frame, Vulkan timestamps over the 60 measured frames put the two foreground ticks at 55.174 ms mean, draw at 0.057 ms and image copy/blit at 0.138 ms. Six follow-up native tick captures show the recurring fluid spike at 70.985 and 74.749 ms, while rigid and particle/heat stages stayed below 1.5 and 0.7 ms. Profiling individual phases to identify the expensive solver work.
