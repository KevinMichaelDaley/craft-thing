---
id: ct-m0uq
status: closed
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

## Notes

**2026-11-26T09:41:19Z**

The authored play clearing in ct-rrwn provides a western loose stone spanning the negative-to-zero chunk seam. The current carry update reconstructs its anchor from low chunk words, and grab searches a fixed window of 64 numeric IDs. Use the clearing as an application regression and exercise bodies outside that legacy ID window as well as independent carry ownership.

**2026-11-26T10:07:46Z**

Grab now resolves exact raster ownership at the aimed cell and validates local reach, body mass, slot and generation. Carry publishes signed canonical chunk anchors with Q16 offsets. Throws preserve their local aim direction, and release retains orientation and prior rotation-lock flags. Right stick grabs, left stick throws, and Back drops on controllers. Verified independent keyboard/controller ownership, disconnect release, stale handle rejection after runtime ID reuse, far signed holds and throws, and the real western clearing stone crossing the seam and reopening dynamic after shutdown. The scene smoke now launches normal play and requests graceful shutdown rather than using a single timing-gated benchmark frame. Final manipulation 13/13, SDL input 5/5, live actions 6/6, clearing 1/1, split 8/8, runtime 3/3, default recipe 2/2 and GPU dependency 9/9 passed. The broad make test rerun continues under ct-il37.
