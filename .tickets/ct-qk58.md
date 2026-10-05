---
id: ct-qk58
status: in_progress
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

