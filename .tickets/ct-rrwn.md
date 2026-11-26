---
id: ct-rrwn
status: in_progress
deps: []
links: []
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

Author stable rooted trees and loose stones using existing terrain/prop contracts. Admission is scoped to scene objects; reopening a save preserves the scene and edits. Cover streamed and negative world coordinates, scene load, harvesting and body ownership.


## Notes

**2026-11-26T05:45:06Z**

First stage archives the implicit session clearing and restores the plain Vulkan runtime scene contract. Distinct authored scene delivery remains open. Existing test_player_runtime baseline fails at the idle screenshot with 297 mismatched channels over 5632 cells.

**2026-11-26T05:47:16Z**

Archived the runtime clearing prototype in Git at 3811d47ffae6934c343c312845d3a28bb8444e34 and eab9fb4737cba7204cd9aff7156acc70cf86a2e2 (play_author_content in src/app/player_session.c). RED reproduced unwanted body admission and the existing 297-channel idle render failure. GREEN and REFACTOR both passed the two test_player_runtime cases including idle, walking, and cast checks. Normal play uses its existing scene terrain, with starter trees and stones pending the authored scene.
