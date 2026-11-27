---
id: ctosd-silg
status: in_progress
deps: []
links: []
created: 2026-11-27T01:32:32Z
type: task
priority: 1
assignee: kmd
parent: ctswc-xr49
tags: [offscreen, vulkan]
---
# Share the Vulkan device across isolated simulation contexts

Restore shared device lifetime support for offscreen physics on the current Vulkan runtime.

## Design

Device, instance, validation and pipeline cache live until the last context releases them. Each context owns its simulation buffers, commands and scene state. Exercise both destruction orders and the partial-construction path.

## Acceptance Criteria

A Vulkan child context on its parent's instance, device and queue steps independently and can be released before or after the parent. Reject invalid arguments and a failed child build without affecting the existing context. Pass core and synchronization validation.
