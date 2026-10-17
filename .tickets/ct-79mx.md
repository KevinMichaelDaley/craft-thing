---
id: ct-79mx
status: in_progress
deps: []
links: [ct-tu55, ct-irzt]
created: 2026-10-17T06:28:35Z
type: task
priority: 1
tags: [vulkan, fluid, performance, parallel]
---
# Bound sustained dense-fluid frame tails at native360

The 180-frame native 640×360 physics/display dense water and grains fixture has no rigid bodies yet reaches 27.863 ms mean, 80.924 ms p95, and 98.386 ms maximum; the final fluid GPU tick is 27.358 ms. Dry and two-concave-body workloads pass. This sustained tail was hidden by the old 30-frame sampling interval.

## Acceptance Criteria

Use the unchanged dense fixture with native 640×360 simulation and display, 180 measured frames, 2 fixed 60 Hz steps/frame, cadence 6, pressure 20; mean and p95 must be ≤33.333 ms. Profile real displayed fluid phases and dispatches; preserve exact conservative recurrences and all literal/state/momentum oracles. No resolution, pressure, cadence, or quality reductions; no CPU solve/readback fallback. Follow TDD with a failing work/performance regression, measured improvement, and full Vulkan/SPIR-V/headless/UI/quarter checks. Preserve existing 480×270 gates and document cold timings separately.


## Notes

**2026-10-17T06:48:09Z**

Per-dispatch attribution confirms transport dominates: late phase 3 has downward transport 18.209 ms, upward 9.352 ms, rightward 61.426 ms; phase 4 leftward transport is 58.770 ms. Pressure remainder is about 0.64 ms. The displayed frame trace reports up to 90.479 ms in phase 3 with zero rigid work. Investigating repeated full-recipient scans and per-source blocker scans while preserving exact source priority and integer flux recurrences; no runtime fluid changes yet.

**2026-10-17T06:50:57Z**

Compiling deterministic RED transport slice: native-game target stays 640×360, while an isolated 640×16 diagnostic compares one directional pass against the literal old recurrence. It has zero cell mismatches and exact water mass, but performs 3,251,248 request queries and 3,251,248 blocker checks for 10,192 sources. Actual participating subgroup is 16 lanes despite local size 32. New budgets require at most 64 request queries per source and linear blocker classification; baseline fails. Validation and synchronization validation report no errors. The explicit sustained performance target also compiles and fails on the dense p95 gate.

**2026-10-17T07:01:36Z**

Found production routing gap: zero-body scenes still selected serial fluid.comp transport, while cooperative fluid_transport.comp was gated by rigid_occupancy_present. Command-recording RED 5b2dff6 fails for ordinary water; candidate routes all four transport modes through cooperative line groups, retaining unused1=0 (original face-only aperture semantics) for no rigid occupancy. Exact coupled runtime fingerprint remains 97a0ea02c0e0a633, mass339325648, zero mismatches with explicit synchronization validation. Sustained displayed acceptance is running; no performance claim yet.

**2026-10-17T07:02:07Z**

Candidate GREEN now exercised by actual body-free runtime: dense native640x360 physics/display180-frame run mean11.604ms,p9517.114ms,max18.584ms,86.174FPS,cold30.788ms,offscreenfluid4.900ms. All three sustained loads also pass unchanged: see build/diagnostics/partial-fluid-native360-all.log; exact concave mean25.861ms,p9532.426ms,cold91.363ms. Two fixed60Hz steps/frame,cadence6,pressure20,scale1 retained. Full explicit synchronization validated headless battery is running before completion; ct-wuci cold14ms gates and existing fracture parallelization remain open.
