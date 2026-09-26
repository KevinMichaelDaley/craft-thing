---
id: dun-o1go
status: closed
deps: [dun-kjp9]
links: []
created: 2026-09-26T05:25:20Z
type: task
priority: 2
assignee: kmd
parent: dun-1v7q
tags: [gpu, scheduler]
---
# Implement fixed-tick pass graph and GPU timings

Record ordered rigid, fluid, and sand dispatch groups with synchronization2 barriers, command buffers, readback captures, and timestamp queries.

## Acceptance Criteria

A capture identifies stage order and GPU duration; each stage reads the preceding stage output without validation errors.


## Notes

**2026-09-26T06:15:13Z**

Implemented one-command-buffer fixed tick graph: rigid integration/occupancy, fluid handoff probe, sand handoff probe. synchronization2 barriers make occupancy/trace writes visible; four timestamp queries capture per-stage GPU duration. Headless stage-order/readback and streamed-window end-to-end tests pass with validation. Probes intentionally stand in for the later Eulerian fluid and material-rule shaders.
