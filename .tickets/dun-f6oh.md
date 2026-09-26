---
id: dun-f6oh
status: closed
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


## Notes

**2026-09-26T05:46:05Z**

C11 worker with bounded queues, procedural load, atomic-file save, shutdown flush, and save/evict/reload end-to-end test implemented. Main-thread Vulkan staging and camera-driven residency integration remain open.

**2026-09-26T06:01:31Z**

Camera-driven worker load/save now feeds the GPU atlas from the main thread. Completion generation tokens reject stale jobs; dirty chunks are downloaded and persisted before slot reuse. Interactive smoke pans away and reloads edits at positive and negative world coordinates.
