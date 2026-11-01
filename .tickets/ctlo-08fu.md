---
id: ctlo-08fu
status: closed
deps: [ctlo-2q4f]
links: []
created: 2026-10-24T22:06:19Z
type: feature
priority: 1
parent: ctlo-d7vm
tags: [fire, smoke, physics]
---
# Add conservative oil and slime phase volume beside water

Introduce continuous bulk oil/slime volume and composition with profile density, conservative face flux and shared free-space capacity. Bulk amount is authoritative and distinct from water.

## Design

Follow design/fire_smoke_reactive_materials.md. Runtime implementation must use RED-GREEN-REFACTOR and repository tests/diagnostics; this turn creates the roadmap only.

## Acceptance Criteria

Pair/seam/closed-vessel tests conserve each ingredient and combined phase capacity, retain overflow and prevent two phases each occupying a free full cell. Density-derived separation, moving-rigid displacement, water drag/heat exchange and save/reload pass. No hardcoded float flag, negative volume, CPU runtime phase scan or fixed global sludge count.


## Notes

**2026-11-01T03:25:43Z**

GPU bulk phase now conserves water/oil in sealed vertical exchange, moves oil/slime across horizontal chunk seams, runs in normal fused and standalone ticks, displaces oil around a rigid occupant, and records closed-vessel retained volume. CPU reference, v10 stream persistence, gas and water shared capacity, and full all/test pass. Remaining: water drag and energy exchange, phase thermal sidecars, and broader moving-rigid/seam reconciliation.

**2026-11-01T03:35:19Z**

Retained oil/slime channels are validated as subsets of authoritative phase stock in GPU imports and versioned packets; malformed but correctly checksummed restore is rejected transactionally. Full all/test passed after RED-GREEN-REFACTOR.

**2026-11-01T07:01:38Z**

Added RED-GREEN-REFACTOR GPU regression for water heat carried by oil/water density exchange (e9ea5f8, cc4e91a, 625e5a6). Bulk pair now transfers the water thermal sidecar in proportion to Q16 water volume; total stored water heat is conserved in the sealed vertical exchange. Full make test under Xephyr :92 passed. Remaining acceptance includes distinct oil/slime heat inventory, water-to-phase thermal exchange/drag, and broader moving-rigid/seam reconciliation; ticket stays in progress.

**2026-11-01T07:07:00Z**

Added RED-GREEN-REFACTOR GPU test for velocity-based water drag on immersed oil (3a162fa, 7f07a2b, 8457a8a). Upward water face flow now entrains mixed oil against gravity, while dry oil at the same face velocity remains below; phase volumes remain conserved. Full make test under Xephyr :92 passed. Still pending independent oil/slime thermal inventories and exchange, persistent sidecars, and broader moving-body cases.

**2026-11-01T07:21:12Z**

Added separate oil/slime excess-energy sidecars to the bulk chunk, BLP2 save packet with BLP1 backward decoder, strict finite import checks, and conservative GPU heat advection alongside phase volume (RED a7f4e60/ae3f36a/b986ba0, GREEN 8c8ee94, REFACTOR e4df489). Shared C/GLSL channel layout now asserts matching stride. Packet, stream, bulk GPU, and full make test pass under Xephyr :92. Remaining: direct water/oil/slime thermal exchange and broader moving-rigid/seam reconciliation.

**2026-11-01T07:30:13Z**

Added RED-GREEN-REFACTOR conserved liquid thermal coupling (dca561b, 33e71e5, 7fb4c77): a dedicated GPU pass uses profile heat capacities/conductivities and exact two-capacity relaxation for contacting water/oil/slime in each loaded cell. Red water-oil test and three-phase conservation test pass; full make test under Xephyr :92 passes. Remaining moving-rigid/seam reconciliation before closure.

**2026-11-01T07:41:30Z**

Final RED fc5d072, GREEN 8763329, REFACTOR 725b2f9: moving kinematic rigid body crossed x63/x64 chunk seam while oil volume and excess heat stayed conserved; rejected zero-stock oil/slime heat in packets and GPU imports. Full make test under persistent Xephyr :92 passed with zero failures. Acceptance criteria now covered by pair/seam/closed-vessel, water drag, density exchange, thermal coupling, persistence, and moving-body regressions.
