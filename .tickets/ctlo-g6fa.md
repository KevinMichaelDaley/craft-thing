---
id: ctlo-g6fa
status: in_progress
deps: []
links: []
created: 2026-10-24T19:21:02Z
type: task
priority: 1
---
# Fuse visible albedo palette refresh and shading

## Acceptance Criteria

One visible-pixel dispatch replaces refresh and shade passes, retaining exact display pixels and palette-generation counters across edits and viewport changes. Keep explicit reference path, no lighting changes, actual Vulkan synchronization validation and displayed full-frame measurement required.

