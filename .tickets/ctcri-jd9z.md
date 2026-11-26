---
id: ctcri-jd9z
status: in_progress
deps: []
links: [ct-1i55, ctcr-uh5f, ctpao-oywm, ctpao-vwpc]
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

**2026-11-26T22:55:10Z**

RED production session fixture injects failures separately in movement, ray, and sprite setup. All three paths returned failure with two published seats and the newly created body and parcels still live. Eleven existing input and cast checks passed.

**2026-11-26T23:04:32Z**

GREEN passed all 14 production session tests, including the individual drive, ray, and sprite failure controls and their retries. Seven ray scheduling cases passed, including append of a new emitter without replacing an active ray. The SDL/Vulkan fault-injection fixture and body, ray, and action binaries compiled.

**2026-11-26T23:13:08Z**

The first native join fixture completed all three rollback checks before its retry assertion read a cached slot bound as a count of live owners. The exhaustive fixture now counts live owner records directly. The separate Vulkan body suite passed 6/6, ray suite passed 3/3 and visible actions passed 8/8.
