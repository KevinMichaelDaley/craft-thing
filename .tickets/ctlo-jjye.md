---
id: ctlo-jjye
status: open
deps: [ctlo-ehqj, ctlo-ao7l, ctlo-yvip, ctlo-hjxc, ctlo-iit1, ctfs-81vw]
links: []
created: 2026-10-24T22:06:19Z
type: task
priority: 1
parent: ctlo-d7vm
tags: [fire, smoke, physics]
---
# Meet displayed full-frame fire smoke and siege acceptance budget

Profile and optimize all three locked feature scenes on this machine. Include ongoing fire/smoke, oil rheology, mechanical contacts, damaged geometry, streaming and presentation.

## Design

Follow design/fire_smoke_reactive_materials.md. Runtime implementation must use RED-GREEN-REFACTOR and repository tests/diagnostics; this turn creates the roadmap only.

## Acceptance Criteria

Each scenario passes two consecutive 1000-frame actual Wayland/Vulkan 480x270 physics/render replays with complete-frame mean and worst below 30 ms, no concurrent build/GPU test and no cadence/geometry/resolution shortcut. Report startup separately, active work, memory growth and conservation. All new and affected unit/integration/visual checks pass; the full suite has no new failures and explicitly reports any pre-existing tracked baseline. Never reclassify a new failure as baseline. No fixed global fire/body limit, silent gas/product dropping or headless-only closeout.

