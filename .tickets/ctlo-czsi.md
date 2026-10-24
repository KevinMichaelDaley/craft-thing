---
id: ctlo-czsi
status: in_progress
deps: []
links: []
created: 2026-10-24T09:49:28Z
type: task
priority: 1
---
# Eliminate exactly redundant SAT axes for axis-aligned contact rectangles

## Acceptance Criteria

GPU frozen eight-axis oracle checks rotated quads, mixed pentagons, rectangular grains and pairs of rectangles over large/fractional coordinates. RED counter reports eight axes; GREEN uses at most six for one rectangle and four for two, with bit-exact hit normals/depth/witness/features. Keep all axes for arbitrary shapes and measure actual graphical frames.


## Notes

**2026-10-24T10:06:42Z**

RED131072 frozen comparisons were exact but rectangle work still8 axes. General rectangular culling then exposed4+11 bit-level roundoff differences, so narrowed eligibility to exact unit-width/unit-height rectangular edges. GREEN196608 frozen comparisons now have zero errors across arbitrary quads, mixed pentagons, fractional/large-coordinate rectangles and unit cells; unit+quad maximum6 axes, unit+unit4, all other rectangles retain8. Actual core/synchronization validation clean; graphical measurement and full contact suite remain.
