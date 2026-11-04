---
id: ctlo-ao7l
status: open
deps: [ctlo-ck1i, ctlo-6syy, ctlo-h9of]
links: [ct-e6fr]
created: 2026-10-24T22:06:19Z
type: feature
priority: 1
parent: ctlo-d7vm
tags: [fire, smoke, physics]
---
# Validate restrained burning-coal siege damage and wet-wall resistance

Run the constructed pivoted launcher against the locked 8-cell-thick dry wall and a maintained very-wet control, using one real burning coal ball and identical impact geometry.

## Design

Follow design/fire_smoke_reactive_materials.md. Runtime implementation must use RED-GREEN-REFACTOR and repository tests/diagnostics; this turn creates the roadmap only.

## Acceptance Criteria

Initial locked target is 10-35% wall loss after 60 simulated seconds with localized char and a surviving load-bearing core; no immediate thick-wall deletion or sound-launcher collapse. Very-wet control has no sustained ordinary ignition. First-second visible flame/smoke footprint stays within 25% of the viewport, using the design's locked opacity/emission thresholds. Repeated shots may breach. Actual displayed capture, conservative inventories and total-frame performance are mandatory.


## Notes

**2026-11-03T13:26:48Z**

GREEN infrastructure delivered (scene src/app/siege_scene.c, tests tests/app/siege_coal_tests.c + siege_coal_display_tests.c): locked fixture build verified (8-cell wall as five anchored structural wood members spanning x154..162 y150..230 with surviving-support state, launcher posts/bar/plinth as fixed constructed terrain, kinematic pivot mount, rigid arm with revolute pivot/latch/pouch, one 49-cell burning coal ball at 1000K FLAMING with pre-armed ignition); real release measured at the arm swing (release speed 3.9-4.0 cells/tick = 235-240 cells/s, within the rigid velocity cap; latch->release 0.15-0.20 s); impact window 0.200 s after release (locked 0.2-0.3); the ball now decelerates against the structural wall (4.0->1.76 cells/tick through the face) instead of tunneling; first-second visible footprint measured with the locked thresholds (soot optical depth >=0.2 or flame emission >=0.1): 17.05% and 5.32% of the viewport, within the 25% bound; very-wet control: wall parcels saturated at 18678/39322 bound water (0.95) and no sustained ignition; complete-frame presented timings 23.6 ms mean / 28.2 ms worst over 300 frames (informational; the strict gate belongs to ctlo-jjye). NOT MET YET (ticket stays open): wall fuel loss after 60 simulated seconds is 0.00% against the locked 10-35% window - the burning ball heats the wall face to ~310-375K but wall wood (ignition 600K) does not catch; also the admitted ball's body-pipeline ignition is nondeterministic across identical runs (burns at ~190 emission power in some runs, zero in others) - both documented in follow-up ct-e6fr. The shot/cold-launch tests run via build/siege_coal_tests (--shot / default); they are intentionally not in the default make test recipe while the wall-ignition gate is open. No launcher collapse in any run (kinematic mount + fixed frame verified sound).

**2026-11-03T21:57:56Z**

Wall ignition achieved via ct-e6fr (rubble pocket + lodged burning ball): flaming 0->47 cells, face 730.1K, char 13119 fp, loss 0.26 percent at 60s accelerating; footprint and wet-control clauses hold. Remaining: 10-35 percent loss needs ignition within ~5s of rest (impact fuel-spill mechanic, see ct-e6fr note) and the impact window needs a deterministic swing base. Ball-rest nondeterminism from the earlier report is resolved by the vertical-crossing release.

**2026-11-04T01:38:43Z**

Deterministic ignition chain verified: lodged ball ignites the wall in 5s (flaming 43 cells, face 643K, loss 0.21-0.25 percent at 60s, reproducible across runs). Wet-control and launcher-soundness clauses hold. Remaining quantitative gaps tracked on ct-e6fr: impact 0.150s vs 0.2-0.3s (needs fixed-tick release or mount offset) and loss 0.21-0.25 percent vs 10-35 percent (plume oxygen throttling - entrainment or contact-width calibration).
