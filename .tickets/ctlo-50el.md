---
id: ctlo-50el
status: closed
deps: []
links: []
created: 2026-10-24T04:42:40Z
type: bug
priority: 1
---
# Refresh paused rigid contacts after terrain brush changes

## Acceptance Criteria

Actual solid terrain edits invalidate paused rigid contact geometry; missed brushes retain no-op scheduling. Compound cavity contact tests pass before and after each edit.


## Notes

**2026-10-24T04:43:34Z**

RED full-suite compound cavity fixture has zero contacts after painting stone into the wall while paused (expected one). Extended fixture also checks replacement by water removes exactly the affected contact. Motion lookup independently fixed in ctlo-3my5.

**2026-10-30T03:54:03Z**

2026-10-29 completion: Existing compound cavity fixture passed only because it explicitly reset body origin after each paint. New RED paused-brush fixture showed zero contacts after stone paint without that reset. GREEN contact readback refreshes only when body_refresh_pending is set; a missed offscreen brush keeps the no-op flag clear. Stone adds one exact compound contact; replacing it with water removes it. Compound body 8/8, contact 10/10, isolated contact recording 3/3; full make test reaches component admission 17/17 then stops at Intel-only reactive_profile_gpu_tests because this host has no Intel hardware compute device.
