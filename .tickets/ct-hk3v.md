---
id: ct-hk3v
status: open
deps: [ct-ortp, ct-8h2f]
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

Abilities are dispatched through extensible ability/action interfaces; no speculative RPG rules. Fire bolt collision deposits appropriate thermal/ignition effect through actual world simulation. Ice storm affects bounded target area through actual cooling/ice material interactions. Avoid repeated triggering due to keyboard repeats unless explicitly designed. Casting uses authoritative assets/player/beaver/beaver_cast.ase, eight unchanged 80x40 RGBA frames: 1-5 windup, 6 casts exactly once, 7-8 recovery. Cast emission must survive skipped samples and step subdivision without duplication. Ownership and target coordinates remain correct across players/cameras. Runtime and deterministic tests validate effects, not cosmetic particles alone.

