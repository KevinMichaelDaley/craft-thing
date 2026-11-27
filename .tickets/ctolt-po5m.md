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

## Notes

**2026-11-27T00:48:45Z**

RED on the live level entry point executed four ticks after two were requested and replayed the completed prefix after a partial equal batch. The production Vulkan API now reports each finished gas tick before image acquisition and publishes combined ticks after the render fence. The level deducts those completions even if presentation fails, and the ten host cases pass. The affected frame tracer and native fault-injection fixtures build. The display tests inject a timeout before acquisition and inject a failure return after real fenced work. The gas case checks that a render-only retry leaves the gas clock unchanged. Record native results before closing.

**2026-11-27T00:59:33Z**

Exact 88c64a1c RTX display regression passed both injected cases. Combined completion count was 0 before acquisition, 2 after the fenced present failure, 0 on render retry, and 2 on subsequent success. Gas count was 2 on the acquire failure and 0 on retry. Vulkan SDK validation 1.3.275 reported older-layer support errors for the existing FIFO latest-ready feature. A rerun with that optional display mode disabled is queued.
