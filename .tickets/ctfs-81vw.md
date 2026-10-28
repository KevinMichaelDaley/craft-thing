---
id: ctfs-81vw
status: in_progress
deps: [ctfs-dmxv, ctfs-mx1x]
links: [ctlo-3ndt, ctlo-ehqj, ctlo-8b67]
created: 2026-10-25T02:54:23Z
type: feature
priority: 1
parent: ctlo-d7vm
tags: [fire, smoke, modular, gameplay]
---
# Deliver the first playable rigid-log bonfire in a constructed stone pit

First gameplay vertical slice after visible thermal and smoke systems. The user's latest instruction supersedes terrain-only scope: use constructed/anchored structural stone members and independent rigid wood logs and kindling naturally stacked inside/above the tinder. Ordinary ground is the only terrain anchor. No painted fixed-terrain pit/log substitutes, kinematic wood, fake rigid visuals, forced ignition or side-separated piles. Add only the real owner/contact/thermal/combustion/residue prerequisites this visible feature requires.

## Design

See the modular delivery section of design/fire_smoke_reactive_materials.md. This user-authorized sequence takes precedence over completing broad horizontal foundation tickets first.

## Acceptance Criteria

RED-GREEN-REFACTOR. Constructed structural stone supports actual independent rigid logs/kindling via contacts, support and gravity. Cold/unlit main logs naturally ignite from enough kindling and one finite starter; no whole-pile burning flags or infinite heater. Actual flame follows thermochemical gas transport. Finite fuel/oxygen/heat/products, seeded ignition, starvation/vent recovery, saturated control and maintained wetting are tested. Burn damage updates real rigid collision/mass/inertia and safely hands ash/fracture to existing owners; save/reload retains supported state/colors. Fullscreen readable interactive presentation shows simulated time, ignition progress, hover inspection and pause/step/reset/exit controls. Automated runs have explicit wall-clock bounds and periodic progress; timeout is failure, not a prolonged silent wait. Preserve normal physical clock/resolution; any accelerated diagnostics must execute the same physical steps and cannot replace normal-clock displayed evidence. Stone/cleared containment limits spread and a fuel bridge permits it. No oil/Greek fire/siege expansion or broad owner-foundation completion claims. Deliver this feature before advancing.


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

**2026-10-27T13:54:51Z**

MANDATORY user final acceptance: ALL3 intended dry main rigid logs must undergo actual fuel-consuming combustion in the autonomous normal-clock test. Ember/kindling burning and nearby flame pixels do not count. Track stable authored log identity through parcel origin/ownership, display N/3 and require3/3 in the permanent bounded displayed test. Main logs start cold/unlit; no forced flags/timers/infinite heaters. Wet control stays separate. Current observed0/3: feature is not complete.

**2026-10-27T14:46:46Z**

User override: ordinary oxygen is a continuously consumed finite reactant, with NO12-percent ignition/extinction cutoff. Surface hazard/rate and airborne carbon kinetics scale continuously with O2; actual stoichiometric admitted mass caps reaction and reaches zero at zero usable O2. Hot ignition memory is independent of oxygen fraction; visible flame requires actual admitted reaction power. RED2b2fbf9/e9a5ad6 cover old-cutoff continuity, zero-O2/no-emission, no overconsumption and generic GPU hazard parity. Initial GREEN evidence CPU13/13, profiles9/9, GPU256-case parity1/1, actual fullscreen oxygen sweep1/1; final zero-emission assertion and vent regression next. All3-log acceptance remains incomplete; finite4-cell1800K starter calibration follows the corrected law.

**2026-10-27T15:01:19Z**

BODY surface visibility: REDb2009df reproduced real dry fuel consumption79Q16/char19 with0/6 changed body pixels. Narrow BODY metadata adapter now actual fullscreen core+sync1/1: localized1/6 marked, unchanged unburnt/authored-grain neighbors, unchanged hot inert/nonreacting and wet controls, paused bit-identical pixels and rotated0.4rad rest-state/mark retention. Captures before/after/paused/rotated in build/screenshots/body_surface_*.bmp; permanent tools/diagnostics/body_surface_contact_sheet.py produces inspected close-up sheet. This is feedback from actual char/reaction power/temperature, not surrounding-flame or whole-body tint. All3-log autonomous gate still incomplete.

**2026-10-27T16:01:47Z**

Current actual fullscreen all-log acceptance after wetting adapters:3/3dry0/3wet at6.4simsec/6.6wall, fuel216/193/48Q16 and successful exit save. Obsolete all-terrain autostart diagnostic superseded by permanent body-based bonfire_display_tests/test_bonfire; its latest bounds/progress preserved in9d39bb9 before removal. Full serial make test exposed stale thermal checkpoint test rejecting purpose2, now validBONFIRE; invalid test updated to enumMAX+1 and focused6/6pass. Full regression remains in progress, not yet a pass.

**2026-10-27T19:49:36Z**

Inline takeover: ordinary flame and rest-mapped ember color now use a generated Planck/CIE temperature lookup with power-controlled brightness and an explicit magical RGB override path; CPU color/power/override tests and both Vulkan shader builds pass. Sustained all-log ignition remains in progress: real heated stone pit reduced ignition to 2/3; hot skin radiation fixed and bounded feedback averaging under displayed validation. Do not close this feature on a transient flare.

**2026-10-27T20:22:13Z**

User clarification: wood should transmit heat sensibly; investigate actual coupling and physical thermal penetration before introducing an arbitrary gameplay speed multiplier. Current fixture expanded to 8 logs and 12 cold kindling pieces per pit (300 cold wood pixels plus 8 finite ember pixels), shared CPU/GLSL layout. All-log sustained acceptance remains failing; do not claim completion from trace reaction or transient flares.

**2026-10-28T01:59:51Z**

Inline implementation checkpoint: 8 cold rigid logs + 12 cold kindling per pit; finite ember only. Current compensation is 2x solid-solid conduction only, not gas, liquid or gas-surface transfer. Removed blanket chemical amplification; wood18kJ/g including retained char, char28kJ/g, airborne reservation back to4000J/g. Added temperature-dependent Planck/CIE visible efficiency, explicit magic-color flag, bounded gray-soot radiative heat exchange, real-inventory flux limits, energy-weighted outgoing and capacity-weighted incoming raster heat allocation. Native focused energy/covered-wood/diagonal tests pass. Full make test completed with only two pre-existing authored_stage failures tracked ctr-ojvy. All-log sustained bonfire acceptance is still IN_PROGRESS; needs a fresh displayed run with the corrected generated-material build dependency before completion can be claimed.
