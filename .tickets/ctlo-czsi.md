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

