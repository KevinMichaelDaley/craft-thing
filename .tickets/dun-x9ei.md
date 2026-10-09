---
id: dun-x9ei
status: closed
deps: [dun-0p7p, dun-cwhn, dun-9qub]
links: []
created: 2026-09-26T05:25:21Z
type: task
priority: 2
assignee: kmd
parent: dun-mi1e
tags: [gpu, rigid]
---
# Solve rigid contacts against terrain and bodies

Generate contacts from nearby material/occupancy cells, resolve impulses/penetration in bounded iterations, and document speed/substep limits.

## Acceptance Criteria

Body rests on terrain, reacts to removed support next tick, and does not penetrate under supported bounds; body-body overlap is resolved.


## Notes

**2026-10-08T06:54:15Z**

Follow-up contact regression to cover: a body whose reference point is in a missing page can still overlap a resident page and another body. The new XPBD solver freezes integration/application for that asleep slot, but contact effective mass/inertia and velocity currently use its stored Body values. Test streamed-page transitions and treat a frozen slot as zero inverse mass/inertia with zero contact velocity while asleep; retain its stored state for reactivation. Also cover support removal and initially overlapping body separation, which are this ticket's remaining acceptance cases.

**2026-10-09T03:10:40Z**

User expanded acceptance: add an actual display-paced Vulkan rigid dynamics test with two distinct concave bodies. Existing convex API cannot represent cavities, so implement bounded compound convex-piece bodies sharing one pose/mass/inertia; validate cavity occupancy/contact exclusion, compound persistence, and visible falling/rotation/body collisions. Keep quarter-native simulation and run the visible scene on this machine.

**2026-10-09T03:33:47Z**

Completed RED/GREEN/REFACTOR. Added support-removal, initial-overlap, streamed frozen-contact equivalence/reactivation, and linear/angular speed-bound regressions. Frozen slots use temporary zero inverse mass/inertia/contact velocity while retaining stored motion; actual substep start poses eliminate false friction displacement. User-requested concave visual dynamics are now implemented: connected non-overlapping convex compounds under one rigid ID and aggregate GPU mass/COM/inertia, piece-wise cavity-preserving raster/contact tests, distinct contact piece IDs, and V4 snapshots with V1-V3 loading. Green stone L and blue wood U fall, rotate, collide with one another and terrain, and settle in a display-paced 640x448/70-chunk Vulkan scene. make test_rigid_dynamics is the standalone entry; test_ui and test_quarter_native include it. Before/impact/settled BMPs, raw RGBA replay, and generated GIF are in build/screenshots/rigid_concave*. Validation: all 140 headless cases passed; full quarter-native suite passed including 13 rigid cases and 6 streamed window cases plus the new visual scene; final full-grid rigid and UI (5 controls, visual scene, 10 smokes) passed after contact feature changes. Vulkan validation and SPIR-V validation were clean. Native presentation smoke measured 60.29 ticks/s and adaptive 61.4 frames/s. Separate warmed two-compound profile measured rigid 7.712 ms and full GPU tick 14.133 ms with empty fluid/granular pools, validation disabled. Dense active rigid/fluid throughput remains in dun-2b08. Automatic isolated component gravity and buoyancy remain specified in dun-spbq.
