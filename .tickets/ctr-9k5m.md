---
id: ctr-9k5m
status: closed
deps: [ctr-9svu]
links: []
created: 2026-10-14T03:12:55Z
type: task
priority: 1
parent: ctr-6fii
tags: [renderer, vulkan, appearance, lighting]
---
# Build conservative GPU occlusion and transmission hierarchy

Build renderer-only opacity/transmission from exact terrain and current rigid/structural coverage, including concavities and thin walls. Parallel hierarchy generation, ownership-aware dirty masks and streaming/known-sky boundary contract.

## Acceptance Criteria

One-pixel walls block, holes pass light, rotated bodies update, unknown pages cannot leak light, albedo brightness does not change shadows. No global single-invocation scene scan; unchanged tiles skip work.


## Notes

**2026-10-14T04:28:32Z**

GPU hierarchy and scene optical-channel generation validated; odd dimensions, thin walls and unknown residency block conservatively. Frame integration uses exact body overlap and material coverage independently of color. Paused scene skips hierarchy/lighting via GPU dirty masks and indirect dispatch. Moving/rotating concave display cases and measured costs remain pending.

**2026-11-28T04:55:34Z**

Real 480x270 Xephyr render: body rotation cleared 141 previously opaque samples and occupied 140 new ones. All 7,320 opacity samples remained exact when restyling the physical shapes as brick and wood rings changed their rendered colors. The 90-frame motion took 0.879s and four paused captures matched. The explicit hierarchy builder still recomputes clean tiles after an isolated change; a focused regression is underway.

**2026-11-28T05:00:15Z**

Fixed the remaining clean-tile gap. A changed leaf marks its parent on the GPU; unchanged parent tiles skip their reductions and unchanged aggregate extrema/residency stop propagation. The RED stamp check failed for the original whole-hierarchy builder. Core/sync GPU checks now pass on a 7x5 odd hierarchy after isolated edits, an unchanged replay, a corner edit, and a residency change; all 19 lighting tests also pass, including one-pixel walls, holes and unknown boundaries. The normal direct-lighting path continues to use its existing optical input without building unused mips. Displayed body motion, style independence and exact physical state were verified in the preceding note.
