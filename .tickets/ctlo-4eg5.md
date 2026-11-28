---
id: ctlo-4eg5
status: closed
deps: []
links: []
created: 2026-10-24T05:49:07Z
type: task
priority: 1
---
# Cache exact polygon vertices in rigid cell overlap tests

## Acceptance Criteria

SAT cell-overlap queries preserve the frozen scalar result for sparse concave mixed-material rotated pixel bodies and partial boundary cases, with at most six geometry fetches per pixel query. Preserve volume/centroid/face oracle results and all runtime geometry; validate displayed frame performance without changing resolution or physical iterations.


## Notes

**2026-10-24T05:54:33Z**

RED compiles and matches13,824 frozen SAT results, but each overlap query fetches geometry up to64 times against the six-fetch bound. Volume/centroid/face oracle remains exact with five fetches. Cache the rotated local polygon and preserve original SAT arithmetic.

**2026-10-24T06:26:32Z**

Array implementation was exact but displayed35.016ms mean /41.911ms worst. Intel compiler probe identified464:587 spills:fills and10256 SIMD8 instructions. Scalar BodyQuad overlap/face vertices and mathematically sufficient8-slot quad clipping storage preserve all55,296 SAT, volume, centroid and face cases exactly with five geometry fetches. New compiled shader has3361 instructions and7:23 spills:fills (zero queue submissions in the compiler probe). Measuring ordinary displayed performance next; no full-frame claim yet.

**2026-10-24T06:37:01Z**

Scalar quad displayed benchmark: 500 real-time Wayland frames, 33.732ms mean /42.668ms worst; four paused captures identical. Geometry read bound achieved, full-frame target still open. Extended instrumented raster reference cold pipeline compilation remains pending; initial typed/generic five-plane/contact comparisons pass.

**2026-11-28T04:23:09Z**

Current synchronization-validated clipping probe passed four rotated sparse/concave patterns: 55296 overlap comparisons, all exact, and at most five geometry reads. The adjoining face-coverage comparison is now bit-exact too (ctlo-pwxh).

**2026-11-28T04:44:37Z**

Displayed quarter-native RTX check passed on the persistent Xephyr server: the stock two-body, sun and four-light renderer fixture completed 90 physics/render frames in 0.907s, with exact masses and four byte-identical paused frames. The separate 180-tick concave dynamics display also passed in 3.233s including fixture pacing and readbacks. Coupled with the 55,296 exact frozen-oracle cases and maximum five geometry fetches, this checks the implemented change at actual resolution and cadence. No isolated timing gain or saved-scene worst-frame result is inferred.
