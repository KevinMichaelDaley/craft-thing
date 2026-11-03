---
id: ct-e6fr
status: open
deps: []
links: [ctlo-ao7l]
created: 2026-11-03T12:23:48Z
type: bug
priority: 1
assignee: kmd
parent: ctlo-d7vm
tags: [fire, ignition]
---
# Fire spread from burning bodies into adjacent cell fuel does not propagate

 Forest-bonfire and siege fixtures show the same runtime gap: fire burns within one rigid body's parcels and cell-owned fuel burns when pre-armed, but a burning body does not ignite adjacent cell-owned wood. Evidence: (1) forest dry-bridge variant: burning pile (flame 40-65 parcels, power 47-230) does not ignite the dry bridge cells one cell away within 120 simulated seconds across runs (build/agents log forest_final.log); (2) forest trees 21+ cells away never ignite (expected for containment), but the 1-cell-adjacent bridge also never catches; (3) siege: burning 49-cell coal ball (1000K FLAMING, pre-armed ignition, ~6.7 fp/tick per parcel) resting on the structural wall heats the face only to ~310-375K over 60 simulated seconds; wall wood (ignition 600K) never ignites, wall loss 0.00% vs locked 10-35% window; (4) siege admitted-ball body-pipeline ignition is additionally nondeterministic across identical runs (burns with ~190 emission power in one run, zero in another). Suspected area: cell ignition stage thermal view for cells adjacent to burning body parcels, and body-pipeline viability temperature (thermal scratch word 9) for admitted authored objects. Files: tests/app/forest_bonfire_tests.c (--spread mode), tests/app/siege_coal_tests.c, src/app/forest_scene.c, src/app/siege_scene.c.

## Design

Reproduce with: ./build/forest_bonfire_tests --spread (expect spread past the cleared ring) and ./build/siege_coal_tests --shot (expect wall fuel loss within 10-35 percent after 60 simulated seconds). Compare the ignition-stage temperature source for cell-owned records adjacent to body parcels against the accepted bonfire starter path; check whether the thermal grid adopts body-parcel heat into neighboring cell samples; check the body-pipeline viable branch scratch temperature write ordering (choose_body_target versus ignition flag evaluation).

## Acceptance Criteria

Dry-bridge forest run shows flaming fuel beyond the cleared boundary within 180 simulated seconds; broken-ring run shows combustion gas passing through the removed wall section; siege hot-coal shot produces 10-35 percent wall fuel loss after 60 simulated seconds with a surviving load-bearing core; the admitted coal ball burns deterministically in every run; no existing containment assertion regresses.

