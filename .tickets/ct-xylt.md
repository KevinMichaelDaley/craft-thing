---
id: ct-xylt
status: open
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

