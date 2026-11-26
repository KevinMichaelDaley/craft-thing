---
id: ct-yctv
status: closed
deps: []
links: [ct-hk3v, ct-vkik, ctisf-f0wy]
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


## Notes

**2026-11-26T05:45:05Z**

Archived prototype in Git history at 3811d47ffae6934c343c312845d3a28bb8444e34 (src/player/abilities.c). RED 5 passed and 3 failed. GREEN and REFACTOR each passed all 8 on NVIDIA RTX A2000. Storm retains bounded water and reactive cooling. Future actual hail needs explicit material, ownership, and lifetime.
