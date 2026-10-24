---
id: ctlo-iit1
status: open
deps: [ctlo-ldb7, ctlo-18l4]
links: []
created: 2026-10-24T22:06:19Z
type: feature
priority: 1
parent: ctlo-d7vm
tags: [fire, smoke, physics]
---
# Expose energy-based shock ignition and conductivity interfaces

Define material/water conductivity and deduplicated electrical-energy exposure events so planned lightning can interact with reactive state without being required for ordinary fires.

## Design

Follow design/fire_smoke_reactive_materials.md. Runtime implementation must use RED-GREEN-REFACTOR and repository tests/diagnostics; this turn creates the roadmap only.

## Acceptance Criteria

Synthetic conductor/water contact events apply their declared finite heat/shock energy once, affect ignition according to material and wetness, and survive owner changes/reload without replay. Insulating and wet controls use explicit profiles, not a universal shocked-is-flammable flag. Full bolt generation and electrical-network propagation remain separately deferred.

