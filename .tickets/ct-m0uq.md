---
id: ct-m0uq
status: open
deps: []
links: []
created: 2026-11-26T07:16:05Z
type: bug
priority: 2
assignee: kmd
parent: ct-exwr
tags: [player, world]
---
# Keep grab, carry and throw precise far from the world origin

The viewport now resolves a canonical chunk-local aim, but src/app/player_session.c and src/player/manipulation.c still flatten actor, body, and reticle positions into absolute single-precision cells. Small reach, carry offsets, and throw direction can be rounded away far from the origin.

## Acceptance Criteria

Use chunk-relative or exact cell arithmetic for grab reach, carry placement, and throw direction. Cover ordinary and far signed chunk coordinates, a streamed seam, and two independent local players.
