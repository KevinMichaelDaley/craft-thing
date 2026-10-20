---
id: ctlo-55mm
status: in_progress
deps: []
links: []
created: 2026-10-19T22:22:54Z
type: task
priority: 1
parent: ctr-ebl5
tags: [vulkan, fluid, performance]
---
# Compact pressure solve work to GPU wet/pressure tiles

Lit water graphical scene remains132.544ms mean. Red-black pressure traverses every resident tile on every sweep although only a small subset holds water. Build a GPU tile list before each scheduled pressure batch from current wet cells or nonzero pressure, retaining all initial/final dense velocity passes and exact recurrence. No CPU walk/readback or cadence reduction.

## Acceptance Criteria

RED empty/localized-water work counts; exact production-vs-dense canonical water and face velocity at each tick; split phase wake correctness; core and sync validation; actual graphical lit-water capture and total frame benchmark. List rebuilt per scheduled batch to include water edited or moved since previous batch. No fixed tile/body cap beyond allocated runtime dimensions.


## Notes

**2026-10-20T16:58:30Z**

GPU sparse pressure GREEN: empty-to-water work counts and insertion between scheduled batches2/2 core+sync; production-vs-generic canonical water and saved face velocities byte-identical after each of3ticks. Actual lit level03+coupled Wayland benchmark110.633ms mean/124.983ms worst versus132.544/147.894 phase-only baseline,60frames, four paused captures identical; inspected actual scene capture. Full suite pending; parent<30ms target remains unmet. New exact app trace shows remaining fluid transport and dirty renderer work dominate.
