---
id: ctowa-0j6g
status: closed
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

## Notes

**2026-11-27T02:08:47Z**

RED reproduced 2097152 bytes for both the two-page workspace and the foreground cell buffer. GREEN passed on RTX A2000 and software Vulkan with core and synchronization validation. The final software Vulkan case used 65536 versus 2097152 bytes, rejected the out-of-range slot, ticked the last partial page, and restored foreground terrain and exact colors into the compact atlas. Default app and quarter-native checkpoint binaries build, host and recording tests pass, and all 143 compiled make test prerequisites built. The final RTX checkpoint replay and the complete make test run are queued.

**2026-11-27T02:15:44Z**

The exact 17d029cf RTX A2000 replay completed with core and synchronization validation. The compact atlas restored exact foreground terrain and color, stepped the last partial page, rejected the next slot, and measured 65536 bytes for its cell buffer against 2097152 bytes for the foreground. The full make test run remains queued.
