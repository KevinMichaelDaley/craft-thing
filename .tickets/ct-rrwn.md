---
id: ct-rrwn
status: closed
deps: []
links: [ctbpa-ccma]
created: 2026-11-26T05:42:30Z
type: task
priority: 1
assignee: kmd
parent: ct-exwr
tags: [player, beaver, world]
---
# Replace procedural playtest clearing with an authored spawn scene

The 3811d47f and eab9fb47 spawn-clearing prototype paints hard-coded trees and stones every time a player session is enabled, uses viewport coordinates for the simulation brush, and runs global rigid admission as a side effect.

## Acceptance Criteria

Author stable rooted trees and loose stones using existing terrain/prop contracts. Scope admission to scene objects and preserve the scene and edits when reopening a save. Cover streamed and negative world coordinates, scene load, harvesting and body ownership.


## Notes

**2026-11-26T05:45:06Z**

First stage archives the implicit session clearing and restores the plain Vulkan runtime scene contract. Distinct authored scene delivery remains open. Existing test_player_runtime baseline fails at the idle screenshot with 297 mismatched channels over 5632 cells.

**2026-11-26T05:47:16Z**

Archived the runtime clearing prototype in Git at 3811d47ffae6934c343c312845d3a28bb8444e34 and eab9fb4737cba7204cd9aff7156acc70cf86a2e2 (play_author_content in src/app/player_session.c). RED reproduced unwanted body admission and the existing 297-channel idle render failure. GREEN and REFACTOR both passed the two test_player_runtime cases including idle, walking, and cast checks. Normal play uses its existing scene terrain, with starter trees and stones pending the authored scene.

**2026-11-26T09:44:17Z**

Fresh --play worlds use two rooted instances of the lossless level-piece-03 tree prop and three individually tagged loose stone bodies. The western tree and two stone shapes cross streamed chunk seams. Scene creation publishes existing edits only via ordinary save handling, and saved worlds skip the clearing import. The real --play regression verified exact terrain and stone ownership, harvested one wood cell, removed one stone, and preserved the cut and surviving IDs after reopening and streaming. The checked-in pack validates and retains 704 wood and 3526 leaf pixels with original colors. Build, scene placement, scoped rigid admission 6/6, terrain prop 7/7, runtime 3/3, fire demo 1/1, default recipe 2/2 and GPU dependency 9/9 passed. The broad test run remains active under ct-il37.
