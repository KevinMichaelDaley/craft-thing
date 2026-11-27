---
id: ctolt-po5m
status: in_progress
deps: []
links: [ctolt-6it2]
created: 2026-11-27T00:41:02Z
type: bug
priority: 1
assignee: kmd
parent: ctfm-w80b
tags: [vulkan, world-clock]
---
# Report completed physics when a batched frame cannot present

The equal-duration level fast path still calls dc_gpu_present_chunks_steps, which can advance the gas-backed world before image acquisition or queue presentation fails. The caller receives only a boolean and may replay a completed batch when it retries.

## Acceptance Criteria

Report the count of completed ticks independently from presentation success. Exercise failure before simulation, failure after completed ticks, and successful combined rendering. Keep the existing one-presentation batch for equal-duration requests.
