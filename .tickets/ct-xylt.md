---
id: ct-xylt
status: in_progress
deps: []
links: []
created: 2026-10-30T09:35:27Z
type: task
priority: 1
assignee: kmd
---
# Reduce cold and fixture bonfire initialization latency

On RTX A2000 X11, persistent Vulkan pipeline cache reduces repeat GPU startup from about 74s to about 2s, but a cold cache still spends tens of seconds compiling physics and thermal pipelines. The half-native bonfire fixture then takes about 7.2s to upload 187 chunks, including 5.0s for reactive imports. Optimize first launch and fixture admission without altering scene state or frame physics.

## Acceptance Criteria

Measure cold and warm startup separately; reduce cold shader compilation and fixture admission with correctness tests for reactive state and restored chunks; retain device-safe cache behavior; run displayed X11 bonfire and record wall times.


## Notes

**2026-10-30T10:10:02Z**

Half-native RTX A2000 warm-cache startup after monotonic-ID fast path and one arena reservation: GPU window 0.893s, 187-chunk fixture terrain 1.519s, colors 0.051s, reserve 0.187s, reactive imports 2.898s, gas enabled at 5.810s. Previous reactive import was 5.039s and fixture upload 7.177s. Full cold shader compilation remains unresolved; frame still 1097ms and tracked in ctfs-5gil. Reactive chunk GPU tests 5/5 and bonfire fixture CPU tests 3/3 pass.
