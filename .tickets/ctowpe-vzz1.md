---
id: ctowpe-vzz1
status: closed
deps: []
links: []
created: 2026-11-27T13:26:19Z
type: task
priority: 2
assignee: kmd
parent: ctcdo-rcs9
tags: [offscreen, streaming, physics]
---
# Carry MPM grains across a shared owner face during native particle substeps

## Design

Use the interior particle grid and ordered two-layer gather across the shared face. Preserve current liquid, rigid and granular coupling, particle IDs, material, moisture and associated reactive ownership. Feed the confirmed result into the existing paired owner checkpoint.

## Acceptance Criteria

Compare bidirectional crossing particles and receiving layers to one native world, conserve stock and momentum, preserve closed behavior when the neighbor is absent, and verify a camera handoff and cold reopen.


## Notes

**2026-11-27T13:48:19Z**

The native split domain matches complete particle and cell records in opposite directions, including both owners' reactive parcel records. The absent-neighbor grain remains in its original tile. Three camera pans, cold reopen with a partial shared cycle, and stale-generation retry preserve the moving grain and its reactive state. The final direct and camera regressions passed with Vulkan synchronization validation on persistent hidden Xephyr :93.
