---
id: dun-f6oh
status: in_progress
deps: [dun-d8gb]
links: []
created: 2026-09-26T05:25:20Z
type: task
priority: 2
assignee: kmd
parent: dun-oa1i
tags: [world, threading]
---
# Run chunk generation, load, and save on a worker thread

Build bounded request/completion queues and a CPU worker for generation, disk load, and dirty-chunk save. The main thread owns Vulkan staging/upload/readback and uses completion tokens to reject stale jobs.

## Acceptance Criteria

Camera movement never waits on disk I/O in the render thread; no Vulkan call occurs on the worker; dirty data is saved before slot reuse; shutdown drains or cancels jobs safely.

