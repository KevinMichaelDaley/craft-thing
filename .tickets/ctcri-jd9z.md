---
id: ctcri-jd9z
status: open
deps: []
links: [ct-1i55, ctcr-uh5f]
created: 2026-11-26T22:48:28Z
type: bug
priority: 1
assignee: kmd
parent: ct-exwr
tags: [player, vulkan, reactive]
---
# Stage a new local player before publishing GPU and session ownership

A genuinely new seat still publishes its canonical body, moves the seat count and replaces input before drive, ray and sprite setup complete. A later allocation failure can leave a new body or leave adapter counts inconsistent. Active ray state can also be lost when the adapter is rebuilt.

## Acceptance Criteria

Inject drive, ray and sprite publication failures independently. The prior seats, active ray history and input remain usable, with no orphan physical body or reactive parcels. A retry admits one complete player with its intended body, bindings, appearance and inventory. Validate the live SDL/Vulkan join path.


## Notes

**2026-11-26T22:51:48Z**

The controller router now stages input in src/app/player_join.c. The remaining failure window starts when a new body is admitted and lasts through drive, ray, and pose publication. Resume with native or fault-injection evidence at those boundaries, including ray history and the authored body parcel ledger.
