---
id: dun-nycg
status: in_progress
deps: [dun-zbeg, dun-f6oh]
links: []
created: 2026-09-26T05:25:21Z
type: task
priority: 2
assignee: kmd
parent: dun-v2vu
tags: [ui, level]
---
# Add interactive material brush and camera controls

Provide material selection, paint/erase brush, pan/zoom, pause, single-step, reset, seed input, and visible chunk loading state.

## Acceptance Criteria

Brush edits affect correct world cells across negative and positive chunk coordinates; pausing and stepping advance exactly one fixed tick; camera crosses chunk boundaries smoothly.

