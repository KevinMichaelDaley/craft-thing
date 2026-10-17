---
id: ct-k6ym
status: in_progress
deps: []
links: [ct-tu55]
created: 2026-10-17T05:25:05Z
type: bug
priority: 1
tags: [vulkan, presentation, synchronization]
---
# Chain acquired swapchain transitions to semaphore waits

Explicit synchronization validation on displayed native640x360 frames reports WRITE_AFTER_READ on acquire/layout transition and PRESENT_AFTER_WRITE from frame0, before diagnostic pair profiling. Frames finish; this is not evidence of historical crash cause.

## Acceptance Criteria

Compiling RED presentation regression proves initial swapchain transfer transition chains from the acquire semaphore transfer wait stage. Preserve mailbox, finite acquire, image semaphores, render fence and unchanged15second watchdog. GREEN displayed synchronization validation has no WSI hazards; full UI/quarter correctness and native timing remain unchanged in quality.

