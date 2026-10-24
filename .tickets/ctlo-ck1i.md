---
id: ctlo-ck1i
status: closed
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


## Notes

**2026-10-24T22:30:10Z**

RED b10aa9f compiled and failed5 missing-fixture cases; GREEN ee657db adds locked manifest and permanent generator/validator,5/5 pass. Actual isolated Wayland1000-frame pre-feature repeats23.284/48.363ms and23.088/55.194ms mean/worst both fail strict tail gate, with stable paused captures and unchanged protected world. Instrumented180 replay23.116/29.122; GPU averages rigid9.790,fluid5.414,MPM4.517,renderer0.690ms, no contact overflow. Native smoke83.6MiB mapped/111.6MiB device. Full regression running; details and frozen units/maps in design/fire_smoke_baseline.md. Final performance acceptance remains open; no fire runtime yet.

**2026-10-24T22:34:35Z**

Milestone validation complete: frozen final manifest SHA fbaf568bd7a4d2693696073ff455afb232843e4eb0dc1021802c07568ceded95, exact little-endian maps and complete phase/carrier metadata export. Greek sludge is finite phase volume in a cup, not a rigid named-fluid ball; coal ballistic target lies within actual velocity/gravity limits. Full suite660 C passes +5 new Python checks, only known ctr-ojvy2 failures, zero compiler warnings. Measurements retained in design/baselines/fire_smoke_prefeature.json; final frame gate remains FAIL/open. This ticket records baseline and locks inputs; downstream implementation proceeds without claiming feature performance success.
