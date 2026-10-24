---
id: ctlo-hjxc
status: open
deps: [ctlo-2q4f, ctpf-iueo]
links: [ctpe-6m89]
created: 2026-10-24T22:06:19Z
type: feature
priority: 1
parent: ctlo-d7vm
tags: [fire, smoke, physics]
---
# Version reactive initial-state prop and scene export channels

Extend validated material/scene authoring with optional initial wetness, thermal state, coatings/composition and reaction metadata, keeping visual colors/emissive independent.

## Design

Follow design/fire_smoke_reactive_materials.md. Runtime implementation must use RED-GREEN-REFACTOR and repository tests/diagnostics; this turn creates the roadmap only.

## Acceptance Criteria

Legacy props retain defaults and exact colors/material/terrain flags. Roundtrip new channels with bounded schema validation, provenance and atomic physical/appearance admission. Animation/reimport cannot spawn free fuel, reset burned matter or replay ignition. Programmatic siege fixtures need not wait for the separate unfinished general all-role authoring feature.

