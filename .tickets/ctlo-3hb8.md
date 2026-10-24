---
id: ctlo-3hb8
status: open
deps: [ctlo-8b67, ctlo-e4y6, ctlo-tk0j]
links: []
created: 2026-10-24T22:06:19Z
type: feature
priority: 1
parent: ctlo-d7vm
tags: [fire, smoke, physics]
---
# Mix and burn finite oil red-slime Greek fire

Implement fictional compound formation from real oil and jelly inventory, temperature-dependent film behavior, fast combustion, finite bound oxidizer and corrosive products.

## Design

Follow design/fire_smoke_reactive_materials.md. Runtime implementation must use RED-GREEN-REFACTOR and repository tests/diagnostics; this turn creates the roadmap only.

## Acceptance Criteria

No ingredient/fuel duplication. Coated foliage/actor proxies retain burning material under water dousing and submersion while water still exchanges heat/momentum/volume. Reaction consumes finite fuel/bound oxidizer and admits gas/residue conservatively, then extinguishes on exhaustion. Matched ordinary-oil control burns longer; wet nearby uncoated controls do not globally ignite. Corrosion and delayed-load volatility follow exposure/contact, not catapult-name timers.

