---
id: ctlo-yvip
status: open
deps: [ctlo-ck1i, ctlo-6syy, ctlo-3hb8, ctlo-h9of]
links: []
created: 2026-10-24T22:06:19Z
type: feature
priority: 1
parent: ctlo-d7vm
tags: [fire, smoke, physics]
---
# Validate sticky Greek-fire siege corrosion dousing and launcher risk

Use the same siege mechanism with ordinary-oil and Greek-fire controls, foliage/moving actor proxies, coin/ingredient samples, water jets/submersion and prompt versus delayed launch.

## Design

Follow design/fire_smoke_reactive_materials.md. Runtime implementation must use RED-GREEN-REFACTOR and repository tests/diagnostics; this turn creates the roadmap only.

## Acceptance Criteria

Greek material sticks/melts and continues finite reaction under water; matched fuel depletes in at most half the ordinary-oil time. Converted valuables remain spent rust/ash across reload. Prompt launch preserves launcher; an exposed dry 10-second delayed load ignites it in at least 48 of 64 locked seeds, with fireproof-carrier control remaining safe. First-second visible coverage is bounded using the design's opacity/emission thresholds; no screen-filling source burst or silent inventory loss.


## Notes

**2026-10-28T02:40:35Z**

User order: begin only after the real catapult/constructed-wall burning-coal module, reuse that same supported launcher, then implement the oil/slime coating, finite Greek-fire chemistry/corrosion and water-dousing controls needed by this visible module.
