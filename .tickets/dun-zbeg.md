---
id: dun-zbeg
status: closed
deps: [dun-kjp9, dun-d8gb]
links: []
created: 2026-09-26T05:25:20Z
type: task
priority: 2
assignee: kmd
parent: dun-1v7q
tags: [render]
---
# Render resident cells and debug overlays

Draw a full-screen Vulkan view of material/fluid/body data at zoom 1 and nearest-neighbor zoom; expose residency and stage overlays.

## Acceptance Criteria

Visible cells map one-to-one to screen pixels at zoom 1; camera offset and scale are correct across chunk boundaries.


## Notes

**2026-09-26T22:08:52Z**

Implemented exact 1x/2x/4x nearest-neighbor Vulkan blits with centered letterboxing and matching mouse coordinates, residency borders, and rigid/fluid/sand cell-output overlay. Streamed negative-chunk and Vulkan validation smokes pass.
