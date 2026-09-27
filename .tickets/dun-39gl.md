---
id: dun-39gl
status: closed
deps: []
links: []
created: 2026-09-27T22:38:29Z
type: task
priority: 0
assignee: kmd
parent: dun-vd9a
---
# Keep simulation buffers in device-local VRAM

## Acceptance Criteria

Hot Vulkan fluid, MPM, chunk and marker buffers use device-local memory; chunk upload/download use explicit staging only for streaming/tests, never per frame; residency and GPU timing verified on RTX A2000.


## Notes

**2026-09-27T22:40:24Z**

RTX A2000 memory types: VRAM heap 12282 MB is device-local but not mapped; host-visible device-local BAR heap 246 MB; current allocator chooses host-visible coherent system RAM heap 193227 MB for all simulation buffers. Full native GPU timestamps: rigid 3.7 ms, fluid 77.2 ms, granular 30.7 ms per tick (six-phase fluid average). No cell/particle copyback in normal frames, but 608 unchanged set_page calls rewrite a 608-entry host-visible slot map each frame.

**2026-09-27T23:14:01Z**

RTX A2000 native smoke: 220.9 MiB mapped VRAM plus 931.1 MiB device-only VRAM for active buffers; 313.5 MiB mapped system memory is streaming/diagnostic staging only. Chunk/particle transfers occur on stream upload/save and explicit tests, not per frame. Native frame rate improved from 8.5 to about 39-43 fps.
