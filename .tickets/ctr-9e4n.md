---
id: ctr-9e4n
status: in_progress
deps: []
links: []
created: 2026-10-18T01:27:03Z
type: bug
priority: 2
---
# Require grounded granular support for terrain component extraction

Extraction support reduction currently accepts any original adjacent granular cell, including seven stray yellow soil metadata pixels inside the level03 crown. Distinguish ground-backed soil from isolated trapped grains without serial scans. Keep genuinely soil-supported loaded stumps fixed and release unsupported crowns.


## Notes

**2026-10-18T01:43:00Z**

User clarification supersedes material-specific support: connectivity includes all terrain solid pixels. After a post-load edit isolates a component, soil/gravel/etc in it must be part of the same rigid body. Removing the uncommitted downward granular support implementation.
