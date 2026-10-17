---
id: ctr-ojvy
status: open
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

