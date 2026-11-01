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

**2026-11-01T08:36:48Z**

Displayed stress benchmark RED c63111f, GREEN e7a33e1, REFACTOR just committed: optional DC_BENCH_SLUDGE fills 10720 Q16 slime cells in a central basin without double water occupancy, runs full physics/render at 480x270 quarter-native and 1920x1080 display. Normal Wayland display, 180 samples: warm sludge runs 1.937 and 1.969 ms/frame versus baseline 1.209 ms/frame; GPU fluid stage 2.126/1.765 vs baseline 0.715 ms. First cold sludge process took 13.1 s, warm second 2.15 s, indicating separate driver initialization cost. Native scene tests 4 pass and full make test under Xephyr :92 passed. Remaining rigid action-reaction and deformation sample guidance.

**2026-11-01T08:43:39Z**

Deformation RED 4929336/a8b32a8, GREEN 6fb7b46/e4c221c, REFACTOR just committed: symmetric 2D deformation-rate shear sample distinguishes physical shear from rigid rotation in CPU reference and GPU flux. 21 GPU and 8 CPU targeted tests passed, full make test under Xephyr :92 passed. Native Wayland 480x270/1920x1080 180-frame sludge stress repeat 1.967 ms displayed frame, GPU fluid 1.778 ms, within prior warm-run variance and below 30 ms frame budget. Remaining rigid action-reaction and integration audit before closure.

**2026-11-01T08:58:40Z**

Vulkan push ABI regression: validation RED showed VUID-07987 because common layout exposed 36 bytes but bulk phase shader requires 84. RED 55e3dbe added focused validation smoke; GREEN f83a909 widened common compute range to 128 bytes; REFACTOR just committed named range and C struct size assert. Khronos validation smoke with Xephyr :92 passes 2 phase cases and zero Validation Error; normal full make test passed. Full fluid-path validation separately reports descriptor binding VUID-08600, tracked as ct-20i5.
