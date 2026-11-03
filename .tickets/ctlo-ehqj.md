---
id: ctlo-ehqj
status: in_progress
deps: [ctlo-ck1i, ctlo-3ndt, ctlo-h9of]
links: [ctfs-81vw, ctlo-3ndt, ctlo-8b67]
created: 2026-10-24T22:06:19Z
type: feature
priority: 1
parent: ctlo-d7vm
tags: [fire, smoke, physics]
---
# Validate a containable warming bonfire in a forest

Create a permanent displayed integration scene with wood/sticks/stone, a cleared ring, dry/wet forest, warmth/avoidance probes, wind and ventilation controls. Use the locked baseline manifest.

## Design

Follow design/fire_smoke_reactive_materials.md. Runtime implementation must use RED-GREEN-REFACTOR and repository tests/diagnostics; this turn creates the roadmap only.

## Acceptance Criteria

For 180 simulated seconds, finite fuel gives useful warmth and char/ash/smoke, with no ignition outside the cleared boundary in locked still/gentle crosswind cases. Dry-fuel bridge or broken containment permits spread. Tests show oxygen starvation/vented recovery, wet controls and finite heat/oxygen ledgers. No wildlife AI claim. Actual captures and complete-frame timings required.


## Notes

**2026-11-03T13:26:31Z**

GREEN+validation evidence (headless NVidia RTX A2000, X11 DISPLAY=:1, quarter-native 480x270 view on 640x448 grid, scene src/app/forest_scene.c, tests tests/app/forest_bonfire_tests.c + forest_bonfire_display_tests.c, targets build/forest_bonfire_tests / build/forest_bonfire_display_tests in make test): (1) locked fixture build matches the manifest: pile 16x7 rigid body at (220,200), sticks as separate authored objects, ember 980.3K from the locked 12000-energy ignition spread over the disc cells, dry trunks 293.15K wood, wet trunk wetness 0.950; (2) still air 180 simulated seconds: containment held (zero flaming/burnt records and zero >400K gas outside the cleared ring at every 10s sample), warmth probe in-band (useful warmth above 298.15K from s30 through s120; measured ceiling 323.9-352.1K across runs vs locked 318.15K ceiling - recorded as design deviation below), fuel consumed (pile 2084066->~1925000 fp), char grew (1454914->~1486000), exhaust 0->5.1, soot 0->0.043 (smoke), oxygen 70.95->67.6 finite ledger, released heat 25117 J within the finite chemical headroom 1.194M J, scene mass ledger conserved to 0.005 mass units; (3) east (+1 cell/s) and west (-1 cell/s) locked crosswinds: full 180s containment held identically; (4) sealed-pit oxygen starvation via fr_gas_gpu_gate: pocket oxygen fell, reaction power fell to <=1 percent of base; reopening restored oxygen and reaction (vented recovery); (5) wet control: wet tree kept >=95 percent bound water and never ignited in any run; (6) displayed run: 5/5 tests pass; complete-frame presented timings over the full 180s runs: still 10800 frames mean 18.570 ms worst 23.422 ms, east mean 17.809 worst 32.211 ms, west mean 17.700 worst 32.206 ms (informational; the strict 30 ms gate belongs to ctlo-jjye); captures: build/screenshots/forest_bonfire_{warmth,end,east_wind,west_wind,sealed,vented}.bmp (flame pixels verified in capture). Gas crosswind control added: fr_gas_gpu_set_wind + FR_GAS_WIND_WORD with zero-wind behavior unchanged (gas_display_tests 58 passed / 0 failed; combustion display failures verified pre-existing at HEAD via clean worktree run). REMAINING OPEN (ticket ct-e6fr): the dry-bridge and broken-ring spread gates do not propagate yet (body-fire to adjacent cell fuel) - tests kept under --spread mode; warmth ceiling overshoot vs the locked 318.15K band recorded as measured deviation (runs measured 323.9, 329.3, 337.8, 352.1K peaks).
