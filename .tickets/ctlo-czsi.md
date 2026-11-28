---
id: ctlo-czsi
status: closed
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

**2026-11-28T04:39:43Z**

The RTX strict oracle exposed 3,255 mismatches on rotated quads against fractional rectangles and 5,653 against unit cells before the fix. The ordered edge traversal and support-face reduction now match all 196,608 frozen cases bit for bit with core and sync validation, including exact feature IDs and witness coordinates. Fractional and unit rectangles each need at most six axes against arbitrary quads and four against another rectangle. Current graphical dynamics run is pending.

**2026-11-28T04:42:40Z**

Displayed 480x270 concave-body test on hidden Xephyr :93 passed. The 180 physical ticks and final paused captures took 3.233 seconds including fixture pacing and readbacks. Both rotating bodies retained exact mass, struck each other and terrain, and all four paused frames matched. The result measures this graphical scene; the separate saved-world frame-budget ticket is still open.
