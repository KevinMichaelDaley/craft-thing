---
id: ctlo-ao7l
status: open
deps: [ctlo-ck1i, ctlo-6syy, ctlo-h9of]
links: []
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

