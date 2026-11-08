---
id: ct-hk3v
status: closed
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


## Notes

**2026-11-08T03:17:54Z**

GREEN and verified. src/player/abilities.c: fr_player_cast_fire_bolt authors a burning coal charge (paint + flaming reactive seeds + authored-rigid admission), wakes it and launches toward the reticle at the capped projectile speed; ignition deposits through the ordinary combustion/thermal pipeline (measured wall strip 264->382 energy at contact, brand path real). fr_player_cast_ice_storm soaks the bounded disc (113 fluid cells for radius 6) and chills reactive parcels inside the radius to 30 percent, outside untouched. Sandbox sessions dispatch bolt/storm on the fire/ice abilities at the player muzzle with per-player targets; ray demo scenes keep the beam. ability_cast_tests 5/0; player family 8/0, 7/0, 4/0, 7/0; full make -k test 205 suites 0 failures. Q repeat suppression rides the control-tick press edges; cast animation is the authoritative eight-frame asset already covered frame-exactly by player_runtime_tests.
