---
id: ct-yctv
status: in_progress
deps: []
links: [ct-hk3v]
created: 2026-11-26T05:35:44Z
type: bug
priority: 1
assignee: kmd
parent: ct-exwr
tags: [player, beaver, vulkan]
---
# Replace playtest hail spawns and cool the complete ice storm footprint

The November 19 playtest added generic rigid bodies under a process-global counter, ignores spawn failures, and leaves them in the world. Storm cooling also hardcodes world coordinates as physical page coordinates and stops at the target chunk edge.

## Acceptance Criteria

Archive the rigid-body hail prototype by recording its exact revision. A live Vulkan cast resolves the canonical target through the active page map, wets and cools the bounded disc across resident chunk seams, and adds no generic rigid bodies. Invalid and unmapped targets leave the world untouched.

