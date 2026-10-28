---
id: ctlo-6syy
status: in_progress
deps: [ctlo-x5gv, ctlo-3ndt, ctfs-0zw5, ctfs-ermw, ctfs-b6cz]
links: []
created: 2026-10-24T22:06:19Z
type: feature
priority: 1
parent: ctlo-d7vm
tags: [fire, smoke, physics]
---
# Build an assembled siege launcher and real burning projectile release

Author supported constructed frame/base parts, independent pivoted arm and a real latch/pouch/release path using existing supported authoring APIs. Add finite coal/char projectile material and ignition.

## Design

Follow design/fire_smoke_reactive_materials.md. Runtime implementation must use RED-GREEN-REFACTOR and repository tests/diagnostics; this turn creates the roadmap only.

## Acceptance Criteria

Dry cold mechanism cycles without collapsing; a burning-coal load launched promptly does not ignite a sound launcher. Launch velocity follows constrained mechanical work and contact/release, without teleporting projectile or welding the arm to terrain. Record flight/impact/recoil and per-part loads; persist mechanism state. Displayed fixture uses real rigid bodies and support graph, not a visual animation.


## Notes

**2026-10-28T02:40:35Z**

User-approved next visible module after ordinary bonfire regression is green and code is pushed: assembled catapult and constructed-wall demo, then Greek fire. Implement real generation-safe two-body pivot prerequisite first, then supported mechanism, real release and finite burning coal shot against dry/wet constructed walls. No fake arm animation or projectile teleport. Work inline; no subagent requested.

**2026-10-28T21:17:40Z**

2026-10-28 resumed branch. Coal/char body mapping and extracted reactive parcel admission now GREEN on ctfs-ermw, including GPU mass and checkpoint tests. Supported catapult frame, real load/release, rigid elapsed clock, burn damage and displayed dry/wet wall fixture remain outstanding under existing child/blocker tickets.
