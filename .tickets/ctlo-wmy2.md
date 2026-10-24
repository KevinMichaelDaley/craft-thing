---
id: ctlo-wmy2
status: closed
deps: []
links: []
created: 2026-10-24T22:06:18Z
type: task
priority: 1
parent: ctlo-d7vm
tags: [fire, smoke, planning]
---
# Design fire smoke chemistry and incendiary siege roadmap

Record the requested phased behavior, architecture, state ownership, numerical decision gates and dependency-linked implementation roadmap in design/fire_smoke_reactive_materials.md. This ticket is planning only.

## Design

Follow design/fire_smoke_reactive_materials.md. Runtime implementation must use RED-GREEN-REFACTOR and repository tests/diagnostics; this turn creates the roadmap only.

## Acceptance Criteria

Cover wet/oiled/shocked/oxidized fuels, heat, variable-density smoke and oxygen, continuous MPM-guided sludge, magical Greek fire and corrosion, true dynamic pivot constraints, all three integration scenes and the actual displayed full-frame budget. Validate links and the dependency DAG; change no runtime.


## Notes

**2026-10-24T22:10:27Z**

Design deliverable complete in design/fire_smoke_reactive_materials.md. Created epic ctlo-d7vm, 22 required child tickets and one independent deferred lightning-propagation follow-up ctlo-ddvz. Validated every new ID, acceptance section, dependency, document reference and reciprocal link; tk dep cycle clean. No runtime source/shader/asset changes or implementation agents. Existing unchanged-runtime full/UI/quarter validation from 52425e8 remains the baseline; no redundant GPU tests run for documentation.
