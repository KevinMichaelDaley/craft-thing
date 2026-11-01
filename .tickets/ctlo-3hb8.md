---
id: ctlo-3hb8
status: closed
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


## Notes

**2026-11-01T16:03:54Z**

Finite co-located oil and red-slime jelly reaction now burns with bounded oxidizer and gas/residue ledger in C reference, terrain GPU, and extracted body GPU. Water-filled zero-void receivers retain products through gas step and conserve water stock. Focused C 5/5, wet matched GPU, submerged terrain/body tests passed. Full make test passed under persistent Xephyr :92. Native quarter-native bonfire two 1000-frame runs passed: mean/worst 18.482/21.601ms and 18.565/23.494ms; paused captures identical. RED-GREEN-REFACTOR commits 7eb3251..cb5b9fe.
