---
id: ctlo-inhv
status: open
deps: []
links: [ctlo-sgja]
created: 2026-10-23T16:59:33Z
type: bug
priority: 2
tags: [vulkan, performance, shaders]
---
# Reduce cold addressed shader initialization latency on Iris Xe

During dynamic gameplay migration, fresh addressed Vulkan contexts took roughly 4–5 minutes at about 2 GB process RSS before the first growth test reached arena relocation; warm reruns reached the same point in seconds. Suspected cold driver compilation, but profile per-pipeline creation to establish the exact source. Keep diagnostic/profiling tools in the repository. Preserve dynamic body storage and GPU physics.

## Acceptance Criteria

Identify expensive creation phases with checked-in timing tools; reduce cold startup substantially on this machine without changing physics, using a fixed body ceiling, or weakening tests. Verify actual Wayland graphical startup and warm frame behavior as well as compile-only probes.
