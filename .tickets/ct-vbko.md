---
id: ct-vbko
status: in_progress
deps: []
links: []
created: 2026-11-06T05:25:33Z
type: task
priority: 2
assignee: kmd
---
# Player character manipulation: grab, carry, throw

G grabs rigid bodies below carry weight near the reticle; hold F throws toward the reticle at throw-force speed; E drops. Carry weight and throw force are player attributes modifiable by stats/magic. Module: src/player/manipulation.c with headless tests.


## Notes

**2026-11-06T06:09:18Z**

GREEN: manipulation module (grab/carry/throw with per-player carry runtime) 8/0, hotbar module 7/0, input bindings G/E/F/R + slots 1-8 wired into dc_players_step with reticle targeting, beaver-body-relative player position, hotbar spell cast injection (slots 1-2 = fire/ice) and material placement (slot 3 = stone block at reticle). Session: grab/drop/throw/edge-detect per tick.
