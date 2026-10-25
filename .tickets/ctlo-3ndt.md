---
id: ctlo-3ndt
status: open
deps: [ctlo-8b67]
links: [ctfs-81vw, ctlo-ehqj, ctlo-8b67]
created: 2026-10-24T22:06:19Z
type: feature
priority: 1
parent: ctlo-d7vm
tags: [fire, smoke, physics]
---
# Apply progressive burn damage to rigid terrain and structural ownership

Connect material consumption and thermal weakening to exact pixel geometry, mass/COM/inertia, section strength, support wakeup and existing fracture/grace rules. Fire removal is a terrain modification.

## Design

Follow design/fire_smoke_reactive_materials.md. Runtime implementation must use RED-GREEN-REFACTOR and repository tests/diagnostics; this turn creates the roadmap only.

## Acceptance Criteria

Tests preserve painted appearance and reactive state while burning arbitrary mixed concave bodies. Removing support extracts the newly isolated component, while loose ash/leafbits remain independent. No whole-body ash replacement, permanently suspended remnant, invalid self-support or duplicate mass. Capacity failure rolls back products and geometry together; geometry/contact publication is correctly ordered.

