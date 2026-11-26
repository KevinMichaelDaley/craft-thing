---
id: ct-q3hh
status: closed
deps: []
links: []
created: 2026-11-26T05:42:31Z
type: bug
priority: 1
assignee: kmd
parent: ct-exwr
tags: [player, beaver, vulkan]
---
# Admit and launch fire bolts through exact addressed projectile ownership

The provisional fire bolt writes absolute chunk coordinates into a physical brush, runs global rigid admission, and guesses the launched body by searching the first 64 IDs. Coal across a seam is only partly seeded, and parcel IDs use process-global state.

## Acceptance Criteria

Use scoped authoring that returns the new owner with matching burning parcels and launch velocity. Test repeated casts, other nearby bodies, streamed seams, rejected casts and actual impact heat.


## Notes

**2026-11-26T06:23:26Z**

The ct-9v5t RED live fixture also reproduced Reactive cell material does not match physical owner when the prototype fire bolt used its old absolute brush address. Hotbar arbitration now dispatches the latched target. Exact projectile admission remains here.

**2026-11-26T07:03:21Z**

Fire bolts now publish one addressed rigid owner with five burning coal parcels and aimed velocity. Admission leaves terrain intact and rejects occupied or unmapped footprints, including repeat casts before the next physics tick. Regressions cover negative streamed seams, grazing-edge precision, parcel counts, unrelated bodies, live keyboard casting, visible cast animation, and wood impact heat. Ability cast 11/11, player actions 5/5, player runtime 3/3, split 3/3, pixel geometry 7/7, reactive body snapshot 2/2, body carve 12/12, default recipe 2/2 and GPU dependency checks 9/9 passed. The broader make test rerun continues under ct-il37.
