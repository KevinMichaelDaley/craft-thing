---
id: dun-mt9o
status: open
deps: []
links: []
created: 2026-09-27T23:22:02Z
type: bug
priority: 1
assignee: kmd
parent: dun-vd9a
---
# Prevent GPU marker count overflow in long native runs

A 60-frame native smoke run at 1920x1080 reproduced GPU marker count exceeds chunk capacity during dirty chunk download on shutdown. The same run at 960x540 completed. The marker append count can exceed DC_MARKERS_PER_CHUNK, so download rejects it.

## Acceptance Criteria

Run native viewport for at least 60 presented and 60 adaptive frames without marker overflow; keep GPU-only per-frame simulation and validate marker count never exceeds per-chunk storage; verify saves and reloads preserve surviving markers.

