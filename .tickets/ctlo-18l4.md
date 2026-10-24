---
id: ctlo-18l4
status: open
deps: [ctlo-2q4f]
links: []
created: 2026-10-24T22:06:18Z
type: feature
priority: 1
parent: ctlo-d7vm
tags: [fire, smoke, physics]
---
# Transport heat wetness and phase-change energy conservatively

Implement thermal storage, conduction, contact/convection/radiation exchange, fuel drying, water wetting and evaporation with explicit environmental sinks. Use accepted simulated time rather than renders or rigid substeps.

## Design

Follow design/fire_smoke_reactive_materials.md. Runtime implementation must use RED-GREEN-REFACTOR and repository tests/diagnostics; this turn creates the roadmap only.

## Acceptance Criteria

Reference and Vulkan tests close energy and water/vapor ledgers, preserve nonnegative state, show finite penetration and heat exposure, and prevent ordinary ignition while a maintained wet control remains wet. Equal-time cadence, pause, body contacts and chunk seams agree within locked tolerances. Water cooling never deletes fuel or grants free heat; exchange work is parallel and localized.

