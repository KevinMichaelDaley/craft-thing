---
id: ctr-ojvy
status: closed
deps: []
links: []
created: 2026-10-17T09:29:27Z
type: task
priority: 2
parent: ctpf-iueo
tags: [physics, authoring]
---
# Finish general authored component plans and absent-normal staging defaults after terrain integration

Combined renderer/exporter/physics checkpoint retains two observed authored_stage_tests REDs: component-plan getter stub and absent-normal default handling. Terrain-only scene placement is implemented independently and verified. Finish shared generic GPU planning without serial whole-component scans and honor absent normal override semantics; retain all-role atomic reservation/publication scope on parent. Full default make test must then be green without ignoring failures.

## Notes

**2026-10-24T01:16:15Z**

Final inline optimization full headless regression again reproduces only the two existing authored-stage failures at tests/vulkan/authored_stage_tests.c:67 and :135.604 reported passing cases; the activity-size and profiling fixture failures from the first run were corrected and pass in this final full rerun. No staging assertion was relaxed.

**2026-10-24T19:27:40Z**

Final inline performance regression rerun still reproduces the same two authored_stage_tests failures at67 (component-plan getter stub) and135 (absent-normal default). These predate this optimization work; no assertions relaxed. All newly corrected fluid/contact and color/albedo tests pass in the rerun so far.

**2026-10-28T02:40:02Z**

Implemented GPU-only staged bounds/count reductions, bounded 8x8 sparse tile election and logarithmic parallel prefix compaction into canonical-root ordered plans. Host readback is one memcpy of completed plans, with required-count reporting on insufficient capacity; no host connectivity/geometry scan. Absent normal override now uses renderer default while explicitly supplied invalid normals still reject. Native tests 6/6, including non-power-of-two 576-pixel input spanning workgroups and three sparse tiles. Full default suite rerun without ignored failures is in progress.

**2026-10-28T02:41:58Z**

Full default make -j1 test completed successfully with zero failures (build/authored_full_regression.log). No ignored failures. Both prior baseline failures are fixed; broader atomic all-role admission remains on the parent ticket.
