---
id: ctlo-k40t
status: in_progress
deps: []
links: []
created: 2026-10-24T18:32:28Z
type: task
priority: 1
---
# Resolve staged rigid colors with one address calculation per word

## Acceptance Criteria

GPU probe verifies exact RGBA for copies in both arena directions and zero-fill markers. Instrumented pool/capacity queries must each be at most one per resolved eight-texel word, preserving all custom colors; actual displayed repeated-cut benchmark must improve without changing simulation cadence or geometry.


## Notes

**2026-10-24T18:44:33Z**

RED: exact RGBA but59 pool/43 capacity queries for3 words. GREEN actual core+synchronization Vulkan probe: exact RGBA in both directions and zero-fill,3 pool/3 capacity queries total. Isolated body-carve suite12/12 passes with particle swapping, preserving colors/mass and all concavities; large190x190 cut9.260ms. A missed-brush timing failed during an overlapping CPU build and passed once isolated; keep timed GPU runs free of builds.
