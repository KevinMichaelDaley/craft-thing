---
id: ctlo-x5gv
status: closed
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


## Notes

**2026-10-28T03:26:18Z**

Implemented parallel GPU revolute Jacobi phases with independently growable BDA storage, generation-bound endpoints, angular limits/friction, finite spring work and breaking capacities. Connected-pair collision suppression uses a growable hash lookup with explicit collide-connected control. Pivot forces/couples feed the existing structural graph. Transactional DCS9 carries validated joints; DCS8 remains unchanged for worlds without joints. Material-local anchors remain fixed across COM changes, with ABI1 migration. Actual fullscreen Wayland Vulkan battery: 10/10 passes; bare anchor error 0.00026 cells; spring peak 0.07998 <= initial0.08; save/reload trajectory matches; 69 joints and body storage growth to256 preserve handles; deleted/reused IDs cannot reconnect; constructed support receives load. Full default suite passed before the COM correction and is rerunning after it. Next visible work is launcher assembly/load/release, then dry/wet constructed-wall fire damage; Greek fire follows.

**2026-10-28T03:30:24Z**

Final full default regression after the material-anchor correction completed successfully with zero failures (build/revolute_release_regression.log). Fullscreen Wayland/Vulkan 10-case battery also passes (build/revolute_complete_display.log); inspected actual capture build/screenshots/revolute_pivot.bmp. This closes the pivot prerequisite only; assembled launcher and dry/wet constructed-wall gameplay remain on ctlo-6syy and ctlo-ao7l.
