---
id: ctlo-qfde
status: in_progress
deps: []
links: []
created: 2026-10-24T10:32:40Z
type: task
priority: 1
---
# Specialize contact polygon storage to four pixel corners

## Acceptance Criteria

Compiled shader reflection RED shows pure pixel contacts still reserve eight corners. Specialization must retain eight for arbitrary convex geometry and use four for pixel-only contacts. Frozen eight-corner scalar manifold oracle and actual graphical tests remain exact; no geometric approximation or new body limit.

