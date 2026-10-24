---
id: ctlo-ehqj
status: open
deps: [ctlo-ck1i, ctlo-3ndt, ctlo-h9of]
links: []
created: 2026-10-24T22:06:19Z
type: feature
priority: 1
parent: ctlo-d7vm
tags: [fire, smoke, physics]
---
# Validate a containable warming bonfire in a forest

Create a permanent displayed integration scene with wood/sticks/stone, a cleared ring, dry/wet forest, warmth/avoidance probes, wind and ventilation controls. Use the locked baseline manifest.

## Design

Follow design/fire_smoke_reactive_materials.md. Runtime implementation must use RED-GREEN-REFACTOR and repository tests/diagnostics; this turn creates the roadmap only.

## Acceptance Criteria

For 180 simulated seconds, finite fuel gives useful warmth and char/ash/smoke, with no ignition outside the cleared boundary in locked still/gentle crosswind cases. Dry-fuel bridge or broken containment permits spread. Tests show oxygen starvation/vented recovery, wet controls and finite heat/oxygen ledgers. No wildlife AI claim. Actual captures and complete-frame timings required.

