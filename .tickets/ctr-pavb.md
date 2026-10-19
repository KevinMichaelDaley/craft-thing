---
id: ctr-pavb
status: open
deps: []
links: [ctr-r0zy]
created: 2026-10-18T00:27:06Z
type: bug
priority: 2
---
# Save the active scene on SIGTERM and SIGINT through normal shutdown

The interactive demo currently exits directly on termination signals and can lose active unsaved edits. Signal handlers should request exit through the main loop, which then flushes the existing scene save path.

## Acceptance Criteria

Termination signals request normal shutdown without Vulkan or file IO in the signal handler; edited active chunks, rigid bodies and colors are saved; regression proves the checkpoint contains the latest edits.

