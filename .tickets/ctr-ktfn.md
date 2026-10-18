---
id: ctr-ktfn
status: closed
deps: []
links: []
created: 2026-10-17T23:29:20Z
type: bug
priority: 1
---
# Packed powders bear rigid loads and yield gradually under excess pressure

## Design

Transmit grounded granular support through a parallel GPU field. Use material density and frictional bearing capacity rather than treating individual grains as disconnected masses. Preserve finite motion for ungrounded grains.

## Acceptance Criteria

Wood and ordinary stone bodies settle on grounded dirt, gravel and sand without tunneling; ungrounded loose grains cannot suspend bodies; overload produces bounded slow settlement with conserved grain mass; Vulkan regression and visual scene.


## Notes

**2026-10-17T23:47:10Z**

User equilibrium correction reproduced: embedded trunk initially y10.5 rises to y-0.0003 over180 full ticks while soil refills its old space. Initial overlap correction on grounded grains injects upward motion and ratchets with particle refill. Add equilibrium and upward COM regressions; preserve gravity/contact balance rather than cold depenetrating the loaded bed.

**2026-10-18T00:01:00Z**

Final focused GREEN: embedded loaded trunk stays exactly y10.5000 through360 full ticks; late drift0 and final vertical speed0, no soil refill, exact2044x78643 soil mass. Core/synchronization validation clean. Grounded contacts preserve initial rest overlap and use zero subyield spring compliance; loose grains keep finite-mass coupling. Eight load cases include wood/stone on dirt/gravel/sand, compound dense wood and1000x stone overload (maximum descent0.0518 cells/tick). GPU support propagation uses pointer doubling, no CPU component traversal or depth cap. Final full suite in progress.

**2026-10-18T00:08:58Z**

Additional RED prevents a regression to hovering: two trapped root grains above a clear air row held the trunk at y10.5. Removed the two-row support shortcut; adjacent occupied rows only. GREEN unsupported trunk falls to y11.484 while supported trunk remains exactly y10.5 through360 ticks (zero drift/speed). Six focused regressions pass, all load cases and mass checks pass. Full level03 core/sync validation passes with zero upward wood COM displacement and zero sampled stone overlap.

**2026-10-18T00:14:15Z**

Final full make test completed on a45dabd: only the two pre-existing authored_stage_tests failures (lines67/135), tracked ctr-ojvy. Six focused regressions pass; exact supported equilibrium, falling air-gap trunk, all eight load cases and exact mass; full authored scene core/sync validation passes. Fresh launcher uses world_chunks_level_piece03_equilibrium and preserves every prior world directory.
