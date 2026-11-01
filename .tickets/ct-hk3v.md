---
id: ct-hk3v
status: open
deps: [ct-ortp]
links: []
created: 2026-11-01T19:29:54Z
type: task
priority: 1
parent: ct-exwr
tags: [player, beaver]
---
# Wire Q fire bolt and targeted E ice storm into material world

Provide two configurable test ability definitions. Q casts a moving fire bolt; E casts an area ice storm at a world target. Mouse targeting for keyboard and controller target intent must have explicit documented semantics.

## Acceptance Criteria

Abilities are dispatched through extensible ability/action interfaces; no speculative RPG rules. Fire bolt collision deposits appropriate thermal/ignition effect through actual world simulation. Ice storm affects bounded target area through actual cooling/ice material interactions. Avoid repeated triggering due to keyboard repeats unless explicitly designed. Casting animation has 4 frames. Ownership and target coordinates remain correct across players/cameras. Runtime and deterministic tests validate effects, not cosmetic particles alone.

