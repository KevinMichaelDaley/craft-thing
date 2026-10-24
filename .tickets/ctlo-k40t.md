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

