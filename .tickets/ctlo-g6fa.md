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


## Notes

**2026-10-24T19:24:25Z**

GREEN actual Vulkan core+synchronization validation: visible-work suite2/2 and full renderer scene suite1/1; exact displayed pixels against the explicit two-pass path, fresh offscreen optical queries, and existing memory budget retained. Default real Wayland level03 repeated-cut1000 frames24.079ms mean29.334ms worst, repeat24.133/29.147. Both pass strict30ms full-frame gate with four identical paused captures.
