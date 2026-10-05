---
id: ct-qk58
status: closed
deps: []
links: []
created: 2026-10-05T00:57:58Z
type: bug
priority: 0
parent: dun-oa1i
---
# Clip chunk velocity transfers at partial viewport edges

Fully resident 1920x1080 benchmark exposed unbounded 64x64 host velocity copies in page mapping and chunk download. Bottom and right partial pages access past viewport buffers.

## Acceptance Criteria

Mapping and downloading partial right/bottom chunks touch only valid viewport velocity cells, preserve cached offscreen state, and never overwrite guard storage.


## Notes

**2026-10-05T00:59:14Z**

RED guard test reproduced writes past 65x65 mapped velocity storage. GREEN clips both upload mapping and download to visible extents and preserves cached offscreen velocity cells. Targeted Vulkan tests pass on Intel Iris Xe with validation enabled; final full-suite rerun underway.

**2026-10-05T01:00:05Z**

Final make test: all 86 tests passed on Intel Iris Xe Vulkan with DC_VK_VALIDATE=1; no validation errors or warnings. Shared extent helper removes duplicated clipping logic.
