---
id: ctlo-2vcr
status: in_progress
deps: [ctlo-08fu, ctlo-18l4]
links: []
created: 2026-10-24T22:06:19Z
type: feature
priority: 1
parent: ctlo-d7vm
tags: [fire, smoke, physics]
---
# Guide continuous sludge with yield-stress MPM rheology

Use MPM stress/momentum/deformation samples to guide continuous oil/slime flow, including yield stress, shear dependence and thermal softening. Quadrature mass derives from authoritative volume.

## Design

Follow design/fire_smoke_reactive_materials.md. Runtime implementation must use RED-GREEN-REFACTOR and repository tests/diagnostics; this turn creates the roadmap only.

## Acceptance Criteria

Reference and GPU tests show load-dependent yielding, shear response and hot/cold softening. Every gather/resample/flux transfer reconciles against the continuous-volume ledger with no duplicate oil mass. Sludge does not inherit granular two-slot arrival blocking. Water/rigid action-reaction, owner transfers and sustained displayed stress fixtures preserve the mechanical clock and frame budget.


## Notes

**2026-11-01T07:56:48Z**

Initial RED-GREEN-REFACTOR increment 898e116/d6d6931, a8cec42, e9a184f: profile-driven regularized Herschel-Bulkley mobility in CPU reference and GPU bulk pair. Cold low-load slime remains at rest, loaded slime yields, hot low-load slime softens and moves; shear-thinning and invalid input reference tests pass. Full make test under persistent Xephyr :92 passed twice. Still outstanding: separate MPM quadrature samples reconciled to continuous volume ledger at gather/resample/flux, action-reaction and owner-transfer fixtures, and sustained displayed stress/frame budget. Ticket remains in_progress.

**2026-11-01T07:59:48Z**

Additional RED a975dff, GREEN 252016b, REFACTOR test pending: reference quadrature regenerates exact Q16 weights and derived mass from the authoritative mobile volume after flux and resampling, including zero-volume clearing; full make test under Xephyr :92 passed. Still need runtime MPM sample coupling, action-reaction, owner transfer, displayed stress and frame budget.

**2026-11-01T08:05:00Z**

GPU shear RED 2391186, GREEN 32d2765 and REFACTOR just committed: matched loaded slime columns remain volume-conservative while transverse coarse velocity gradient increases flow through profile shear thinning. Persistent Xephyr :92 full make test passed. Remaining runtime momentum/deformation quadrature, water/rigid action-reaction, owner transfers, and displayed stress/frame budget.

**2026-11-01T08:14:36Z**

Momentum RED 9058555/ccb827a, GREEN 9c1611e/14e8046, REFACTOR just committed: BLP3 stores independent phase momentum sidecars without storing mass; BLP1/BLP2 restore zero momentum. GPU flux carries momentum proportionally with conserved Q16 phase volume, orphan momentum rejects transactionally on packet/import. Targeted packet 17 and GPU 17 pass; full make test under persistent Xephyr :92 passed. Still need water/rigid action-reaction, deformational sample guidance, owner-transfer and displayed stress/frame budget.

**2026-11-01T08:21:28Z**

Water-phase action-reaction RED e733c6d, GREEN 638fde1, REFACTOR just committed: GPU co-cell drag conserves sampled water-face plus phase momentum, carries oil momentum with flux and deposits inelastic kinetic loss as heat using the 1 cm cell mass/velocity conversion. 18 GPU tests and full make test under persistent Xephyr :92 passed. Remaining rigid response, deformation quadrature, owner transfers, sustained displayed stress/frame budget.

**2026-11-01T08:29:48Z**

Momentum-guided flux RED e10eb47, mixing heat RED 7152171, GREEN 8b5c802, REFACTOR just committed: phase sample momentum biases flow in its signed direction; transferring phase mass carries corresponding momentum; inelastic merge uses reduced-mass relative kinetic energy deposited as phase heat. Moving rigid across chunk seam conserves phase volume, heat and momentum; horizontal opposite-momentum fixture conserves kinetic+thermal energy. 20 targeted GPU tests and full make test under Xephyr :92 passed. Remaining persistent deformation-guided stress, rigid action-reaction and sustained displayed frame budget.
