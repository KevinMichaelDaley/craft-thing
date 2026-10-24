---
id: ctlo-ddvz
status: open
deps: [ctlo-iit1, ctlo-r2ap]
links: [ctlo-d7vm]
created: 2026-10-24T22:06:19Z
type: feature
priority: 2
tags: [fire, smoke, physics]
---
# Propagate future lightning energy through conductors and water

Deferred follow-up: connect a future lightning source to a GPU conductivity network and distribute finite electrical energy through connected material and water. This is not required to ship the fire milestone.

## Design

Follow design/fire_smoke_reactive_materials.md. Runtime implementation must use RED-GREEN-REFACTOR and repository tests/diagnostics; this turn creates the roadmap only.

## Acceptance Criteria

Conducting/insulating and wet/dry branches, chunk seams and moving-body contacts route energy with defined source/ground/boundary conditions. Account for Joule heat and ignition exposure without using a connectivity-only unlimited shock flood. Deduplicate events and persist relevant state. Actual lightning bolt generation and wildlife behavior remain independent features.

