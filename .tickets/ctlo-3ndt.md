---
id: ctlo-3ndt
status: in_progress
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


## Notes

**2026-10-29T16:20:00Z**

2026-10-29 implementation: painted battlement cells already use the terrain combustion/ash path; the missing special case was constructed wood members (including catapult mast), which were excluded from thermal admission. Added reactive wood parcel ownership, continuous constructed-body mass/COM/inertia feedback, partial heat and fuel weakening of structural members, spent-pixel carve/loose-ash handoff with capacity retry, and checkpoint validation. Focused tests pass: component admission 13/13, catapult scene 10/10, display 2/2. Full make test still running.

**2026-10-29T16:29:01Z**

2026-10-29 verification: constructed wood reactive ownership, partial heat/fuel weakening, mass/COM/inertia feedback, checkpoint, capacity-safe carve and loose ash handoff pass focused component admission (13/13), scene (10/10), display (2/2); normal lit 256x128 demo completed 205/205 frames, exit 0. Full make test passed all suites before reactive_profile_gpu_tests, which cannot run on this host because no Intel hardware compute device is available. Ticket remains in progress for broader mixed-concave/support extraction acceptance; ct-79vm tracks converting scene battlements from painted terrain to constructed assembly.

**2026-10-29T17:36:17Z**

2026-10-29 continuation: ct-79vm completed the far-side battlement conversion to supported constructed wood. Body combustion neighbor lookup now uses constructed thermal grids, brief body-body contact transfers heat, and small ignited parcels produce finite fixed-point reaction. Scene/display/component/feedback suites pass. Broad mixed-concave/support extraction acceptance remains open. Full make test stops only at the no-Intel-device reactive_profile_gpu_tests host gate.
