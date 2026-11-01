---
id: ctfm-w80b
status: open
deps: []
links: []
created: 2026-11-01T19:00:27Z
type: task
priority: 1
assignee: kmd
tags: [offscreen, vulkan, merge]
---
# Port archived offscreen simulation branch to current Vulkan runtime

The 2026-09-30 feature/offscreen-simulation branch is merged for Git ancestry. Its runtime implementation could not be safely auto-merged into the 2026-11-01 main code: 18 conflicts across GPU context, device ownership, fluid/MPM shaders, and app level; the tentative merge failed shader compilation (COLOR_STONE, node_position, mud) and current GPU compilation (removed owns_device/slot_capacity fields). The branch commit 7347531 and its full history remain available as the implementation reference.

## Design

Rebuild offscreen workspaces on current dynamic body storage and Vulkan device abstractions. Port chunk capture, world-time cadence, gas and fluid boundary transfer, MPM elapsed-time scaling, sleeping cache/prefetch, and tests incrementally with RED-GREEN-REFACTOR. Keep current renderer, body storage, and reactive state behavior.

## Acceptance Criteria

Offscreen simulation code is active in current build; two-screen pan, seam transfer, equal world time, cold chunk return, conservation, and native frame budget tests pass. Full make test and actual display run pass with no compile warnings.
