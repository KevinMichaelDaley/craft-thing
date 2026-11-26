---
id: ct-q3hh
status: open
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
