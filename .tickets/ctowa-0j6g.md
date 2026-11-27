---
id: ctowa-0j6g
status: in_progress
deps: []
links: []
created: 2026-11-27T01:56:45Z
type: task
priority: 1
assignee: kmd
parent: ctswc-xr49
tags: [offscreen, vulkan]
---
# Size offscreen GPU atlases to their own page grids

Shared contexts currently allocate and address the full foreground atlas even for one tile. Restore per-context capacity against the current shader, body and checkpoint layouts.

## Design

Derive shared capacity from the rounded page grid up to the configured slot limit. Keep the default foreground capacity. Propagate each context's capacity through resource ranges, shader dispatches, bounds and live metadata while preserving checkpoint format limits.

## Acceptance Criteria

Vulkan tests prove a small shared workspace actually allocates smaller buffers, reaches its last partial page, rejects unavailable slots, and steps with exact channel readback. Existing foreground and checkpoint paths build and run with the same defaults.
