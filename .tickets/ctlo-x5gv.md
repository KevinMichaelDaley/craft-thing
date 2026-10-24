---
id: ctlo-x5gv
status: open
deps: [ctlo-wmy2, ctlo-ck1i]
links: []
created: 2026-10-24T22:06:19Z
type: feature
priority: 1
parent: ctlo-d7vm
tags: [fire, smoke, physics]
---
# Add generation-safe dynamic rigid-body revolute constraints

Implement a true two-body pivot for a rotating siege arm, separate from static structural pinned supports. Include finite limits/friction/capacity, release and persistence.

## Design

Follow design/fire_smoke_reactive_materials.md. Runtime implementation must use RED-GREEN-REFACTOR and repository tests/diagnostics; this turn creates the roadmap only.

## Acceptance Criteria

Anchor translation is constrained while rotation remains free; off-center loads, recoil and repeated cycles remain stable with measured constraint error. Work/momentum and actuation/spring-energy accounting are explicit. Carving/fracture/deletion and generation reuse cannot bind an unrelated body. Save/reload and growing body/constraint storage preserve joints without fixed global counts or CPU per-frame solve.

