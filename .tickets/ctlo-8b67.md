---
id: ctlo-8b67
status: open
deps: [ctlo-r2ap]
links: []
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

