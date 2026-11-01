---
id: ct-ortp
status: open
deps: [ct-2p38, ct-xsll]
links: []
created: 2026-11-01T19:29:54Z
type: task
priority: 1
parent: ct-exwr
tags: [player, beaver]
---
# Integrate beaver movement, jump and world collision into live application

Connect player action intents to live simulation and renderer. Keyboard A/D moves horizontally and Space jumps; establish controller-neutral movement and target intents. Resolve current editor camera/input conflicts explicitly.

## Acceptance Criteria

Beaver is visible in actual application and moves/collides against existing material terrain and rigid world. Grounded jump, release/press semantics, frame-rate-independent simulation, streaming edges and spawn safety covered. Controls are documented; camera follows player without unintended editor actions. Shared interfaces work for multiple player instances.

