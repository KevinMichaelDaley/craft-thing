---
id: dun-vz9y
status: closed
deps: []
links: [dun-u68k]
created: 2026-09-30T05:12:57Z
type: task
priority: 2
assignee: kmd
parent: dun-p9g9
tags: [gpu, gas, streaming]
---
# Transfer gas across visible/offscreen GPU workspace boundaries

Gas now moves deterministically across resident chunk seams, and edge gas requests neighboring chunks. The existing workspace boundary pass transfers water, momentum, markers, and MPM grains but has no gas intent/commit across independent GPU contexts. Extend the conservative boundary handoff to gas without per-frame CPU copyback.

## Acceptance Criteria

A painted gas plume crosses visible-to-offscreen and offscreen-to-offscreen workspace boundaries, conserves exact gas cell count, and continues rising after camera pan; no normal-frame CPU readback.

