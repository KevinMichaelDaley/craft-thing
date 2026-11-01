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
