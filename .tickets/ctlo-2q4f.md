---
id: ctlo-2q4f
status: in_progress
deps: [ctlo-ldb7]
links: [dun-sndp, dun-i1zd]
created: 2026-10-24T22:06:18Z
type: feature
priority: 1
parent: ctlo-d7vm
tags: [fire, smoke, physics]
---
# Persist reactive state across terrain particles rigid pixels and coatings

Implement versioned owner-specific sidecars for thermal energy, fuel/char, water, coating composition, oxidation/corrosion, stable ignition-hazard state and bound oxidizer. Do not steal leaf color or loaded-terrain bits.

## Design

Follow design/fire_smoke_reactive_materials.md. Runtime implementation must use RED-GREEN-REFACTOR and repository tests/diagnostics; this turn creates the roadmap only.

## Acceptance Criteria

Exact state survives extraction, carving, fracture, MPM pool swaps, negative-coordinate streaming, save/reload and owner-generation changes. Atomic admission failure retains all source inventories; old saves preserve existing moisture/colors/ownership and initialize ambient reaction defaults. Corrupt/newer input rolls back. No fixed body/fire cap, per-frame CPU scan or hot-path allocation.

