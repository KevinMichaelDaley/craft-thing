---
id: ctfs-81vw
status: in_progress
deps: [ctfs-dmxv]
links: [ctlo-3ndt, ctlo-ehqj, ctlo-8b67]
created: 2026-10-25T02:54:23Z
type: feature
priority: 1
parent: ctlo-d7vm
tags: [fire, smoke, modular, gameplay]
---
# Deliver the first playable terrain wood bonfire with wet and containment controls

First gameplay vertical slice after visible thermal and smoke systems: stationary terrain wood/sticks inside a stone pit in the material-placement demo. Include only the prerequisites needed for ordinary finite-fuel fire, smoke/oxygen, gradual residue and wet controls; reuse existing safe ownership handoffs.

## Design

See the modular delivery section of design/fire_smoke_reactive_materials.md. This user-authorized sequence takes precedence over completing broad horizontal foundation tickets first.

## Acceptance Criteria

RED-GREEN-REFACTOR. Player can ignite the pile, see actual flame/smoke, feel measured warmth, douse ordinary fuel and observe progressive char/ash. Finite fuel/oxygen/heat/product budgets, stable seeded ignition, oxygen starvation/vented recovery and maintained-wet controls are tested. Stone/cleared containment limits spread; a fuel bridge permits it. Ash stays loose and authored colors/loaded-terrain semantics remain correct. Provide a repeatable interactive demo, permanent displayed integration tests/captures and complete-frame performance evidence on this machine. No cosmetic-only fire, instant whole-component burning, future-material expansion or completion claim for all owners, forest scenario, Greek fire or siege features. Continue to the next visible feature only after this slice is usable, validated and pushed.


## Notes

**2026-10-25T02:56:00Z**

Start after the smoke/ventilation box is delivered. This is the first contained gameplay fire, not a claim that arbitrary-body combustion, all forest scenarios, oil or siege mechanics are complete.

**2026-10-26T22:18:17Z**

Starting immediately after user-accepted smoke delivery3495f65. ONLY this visible slice: terrain wood/sticks bonfire in stone containment, real finite fuel/O2/heat/products, visible flame/smoke/warmth and dry/wet controls. Reuse delivered thermal/gas/persistence adapters and add only needed prerequisites; no return to smoke microoptimization or future oil/siege/Greek-fire scope.

**2026-10-26T22:25:58Z**

Pushed RED 955b0bf and GREEN 684ef60: ordinary combustion reference contract, finite substrate fuel/O2, char/ash/soot/exhaust ledger, retained sensible and released chemical heat, seeded persistent hazard, wet/starved/buried controls and transactional invalid input. New 5/5 tests; existing reactive state 9/9, profiles 9/9, thermal 6/6. Runtime GPU connection, visible bonfire, loose ash, controls and displayed acceptance remain in progress; no gameplay completion claimed.
