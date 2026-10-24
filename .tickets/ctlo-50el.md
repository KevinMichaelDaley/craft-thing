---
id: ctlo-50el
status: in_progress
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
