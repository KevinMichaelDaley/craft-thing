---
id: dun-mt9o
status: closed
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


## Notes

**2026-09-27T23:37:48Z**

User reports water falling across the level and camera crash in 187-slot half-native view. Found marker.comp reset/advect/finalize loops hardcoded to 64 slots, while seed dispatch covers all cells. This leaves stale marker counts and guides in slots >=64 and likely causes save/download overflow during camera movement.

**2026-09-27T23:47:18Z**

Fixed marker reset/advection/count clamp dispatch for all DC_GPU_CHUNK_SLOTS and culled markers away from live wet/dry interface. Repro slot 64 marker count fell from 1377 stale markers to 501 after 80 fluid steps; GPU chunk save/reupload preserved count. Native smoke passed 60 presented + 60 adaptive frames plus 120 further ticks; half smoke passed same plus pixel panning across three chunk boundaries. Full make test, test_ui, test_half_native passed; half adaptive 62.1 fps.
