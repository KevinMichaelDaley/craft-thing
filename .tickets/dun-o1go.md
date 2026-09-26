---
id: dun-o1go
status: in_progress
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

