---
id: ctr-nxdp
status: in_progress
deps: []
links: []
created: 2026-10-17T18:51:55Z
type: feature
priority: 1
---
# Retain detached authored pixel colors and save/load all color channels

## Acceptance Criteria

Custom RGB stays attached to body rest pixels through extraction, arena relocation and fracture. Versioned checked saves restore terrain/body/water colors, including chunk eviction/reload. Reopening scene does not stamp over saved edits. Legacy saves load safely; malformed color payloads fail atomically.

