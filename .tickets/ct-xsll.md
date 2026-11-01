---
id: ct-xsll
status: in_progress
deps: [ct-2p38]
links: []
created: 2026-11-01T19:29:54Z
type: task
priority: 1
parent: ct-exwr
tags: [player, beaver]
---
# Author placeholder beaver animation and persistent material pixel bindings

Create recognizable beaver placeholder art with robe, hat, fur, teeth and tail, plus 6 walk, 8 chewing and 4 casting frames. Supply per-pixel material assignment and stable body/equipment identity across animation.

## Acceptance Criteria

Assets load through supported pipeline with exact frame counts and usable frame timing. Fur, robe and hat have explicit material coverage; transparent pixels are not solid. Reactive state persists by player and material/rest identity through frame changes, flips and movement: no reset by replacing frames. Rendering reflects local singeing, wetness and burning, backed by actual existing reactive simulation. Different player instances never share mutable pixel state. Include asset checks and runtime visual evidence.

