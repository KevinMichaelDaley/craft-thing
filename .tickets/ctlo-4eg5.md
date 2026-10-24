---
id: ctlo-4eg5
status: in_progress
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
