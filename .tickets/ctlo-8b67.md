---
id: ctlo-8b67
status: closed
deps: [ctlo-r2ap]
links: [ctfs-81vw, ctlo-3ndt, ctlo-ehqj, ctlo-iit1, ct-bk2l]
created: 2026-10-24T22:06:18Z
type: feature
priority: 1
parent: ctlo-d7vm
tags: [fire, smoke, physics]
---
# Implement deterministic stochastic ignition and finite fuel combustion

Model heating/drying, ignition, char/smouldering/flaming and ash cooling using material profiles and local heat/oxygen. Aggregate exposure rather than scanning every flame pair.

## Design

Follow design/fire_smoke_reactive_materials.md. Runtime implementation must use RED-GREEN-REFACTOR and repository tests/diagnostics; this turn creates the roadmap only.

## Acceptance Criteria

Persistent per-parcel integrated hazard matches 1-exp(-lambda*dt), seeded ensembles and equal-time cadence; extraction/camera changes cannot reroll it. Reactions reserve shared oxygen/products before atomic commit and conserve fuel plus oxidizer into finite char/ash/soot/vapor/exhaust and heat. Wet/dry/oiled/shocked/oxidized controls, sealed oxygen starvation and vented reignition pass. Burning progresses from exposed surfaces, never instantly through an entire component.


## Notes

**2026-11-01T14:12:07Z**

Completed ordinary deterministic ignition and finite wood combustion prerequisite. Existing reference/GPU coverage proves persistent per-parcel threshold and equal-time cadence, finite fuel/oxygen/products/heat, surface progression, sealed starvation and vented recovery, wet/dry and oxidized profile controls. New RED/GREEN/REFACTOR work admits finite oil-coated wood to ordinary substrate ignition while retaining film inventory for later Greek chemistry (a7d69c5,2ce672f,3359a1d,f2c5dfb,88d9c89,f0221b8); a 4096 parcel seeded ensemble matches 1-exp(-lambda dt). Reserved impact energy now converts exactly once into compensated parcel heat before thermal exchange/ignition for reference, terrain and authored rigid owners (07e8c73,267a701,3622a5f,0ddcb38,ccf4837). Updated oxygen GPU assertion includes the stored fractional fuel reaction budget (42dbdad). Focused reference 18/18, GPU oil/ash/shock/rigid shock/oxygen/vent all pass under persistent Xephyr :92; final make test passes. Native quarter-resolution 1000-frame bonfire mean 18.347ms/worst29.550ms passed; repeat mean18.335ms/worst30.052ms missed the 30ms worst gate by0.052ms at frame171, consistent with the preexisting intermittent display-present tail tracked in ct-bk2l. Impact energy producers and conductivity interfaces remain ctlo-iit1; film fuel chemistry is ctlo-3hb8.
