---
id: dun-6qqa
status: closed
deps: []
links: [dun-ywmw]
created: 2026-09-26T08:47:21Z
type: bug
priority: 1
assignee: kmd
parent: dun-9e5b
tags: [gpu, fluid, streaming]
---
# Simulate offscreen halo chunks to remove viewport-edge fluid walls

## Design

Keep at least one resident, simulated chunk margin beyond the displayed camera window; stream and persist those chunks on the worker. Render only the camera crop. Pressure and transport must treat loaded halo cells as neighbors across every displayed edge.

## Acceptance Criteria

Water crosses the displayed top, bottom, and side edges without loss or reflection; camera movement retains mass and velocity across chunk handoff; no frame readback is added.


## Notes

**2026-09-26T09:13:38Z**

Implemented 24 resident simulation chunks around an 8-chunk camera, center-only rendering/readback, GPU velocity shift on panning, v3 chunk velocity persistence, and dirty halo brush edits. Headless four-edge mass test, camera cell-for-cell replay, streamed save/reload, full tests, long UI capture, and Vulkan validation pass.
