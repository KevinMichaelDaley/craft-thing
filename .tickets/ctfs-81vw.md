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

**2026-10-26T23:09:39Z**

Runtime checkpoints pushed through d35b1d7: actual GPU wood and char burning, finite shared oxygen/products, persistent sub-Q16 progress (ABI4 same28-word layout, legacy1-3 migration), displayed state-driven flame and actual gas warmth, dry/wet stone-pit scene and tools/run_bonfire. Displayed6s ignition heats293.15->836.264K and fuel39322->39186; air warmth464.158K. Exact paused capture; fuel/hazard/probe charge survive full save/reopen; wet control loses no fuel. Loose ash native4/4: exact2000-Q16 mass, actual MPM fall, full DCS restore/re-admission; focused core/sync4/4 before the small unsupported-reservoir guard, and current native4/4 after it. Latest placement water pour also extinguishes the dry starter (water576, flags0, fuel39016). Remaining acceptance: char appearance, containment/fuel-bridge evidence and complete displayed frame timings; milestone remains in progress.

**2026-10-27T01:13:23Z**

Pushed transported reaction-power flame checkpoint8ef1640: no vertical mask; displayed native7/7 plus core/sync validation. All-face gas/solid heat regression fixed1f0c553. Practical reaction cache regression fixed7c427be: four displayed1000-frame runs now dry21.354mean24.152worst and21.242/24.309; wet21.280/23.743 and21.242/23.880ms, versus old40.956-44.532mean49.258-85.320worst. User requires autonomous finite ember/kindling ignition with cold main logs, not whole-pile lit flags. Authoring/autostart is currently RED and in progress; containment/bridge spread also remains RED. No bonfire completion claimed.
