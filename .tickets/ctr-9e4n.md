---
id: ctr-9e4n
status: open
deps: []
links: []
created: 2026-10-18T01:27:03Z
type: bug
priority: 2
---
# Require grounded granular support for terrain component extraction

Extraction support reduction currently accepts any original adjacent granular cell, including seven stray yellow soil metadata pixels inside the level03 crown. Distinguish ground-backed soil from isolated trapped grains without serial scans. Keep genuinely soil-supported loaded stumps fixed and release unsupported crowns.

