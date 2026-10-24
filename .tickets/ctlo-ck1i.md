---
id: ctlo-ck1i
status: in_progress
deps: [ctlo-wmy2]
links: []
created: 2026-10-24T22:06:18Z
type: task
priority: 1
parent: ctlo-d7vm
tags: [fire, smoke, physics]
---
# Lock fire siege fixtures reference budgets and diagnostics

Establish reproducible pre-feature world copies and scenario manifests before implementation. Define physical clock adapters and normalized material/thermal units. See the design performance and integration sections.

## Design

Follow design/fire_smoke_reactive_materials.md. Runtime implementation must use RED-GREEN-REFACTOR and repository tests/diagnostics; this turn creates the roadmap only.

## Acceptance Criteria

Lock maps, seeds, impact energy, wetness controls, 180-second bonfire and 60-second wall windows, coverage/damage limits and 64-seed delayed-load ensemble. Capture current actual Vulkan/Wayland 480x270 baseline, startup versus sustained cost, frame/GPU/memory counters and conservation ledgers. Permanent repo tools only; no threshold relaxation or headless-only performance acceptance.

