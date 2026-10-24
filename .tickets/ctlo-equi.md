---
id: ctlo-equi
status: in_progress
deps: []
links: []
created: 2026-10-24T09:59:14Z
type: task
priority: 1
---
# Wait for brush submissions with the render fence on Wayland

## Acceptance Criteria

Real recreated/hidden graphical window test paints before every frame. RED exposes queue-wide idle waits and absent brush fence waits. GREEN waits each brush submission fence without queue/device idle, preserving synchronous paint visibility, command reuse, semaphores and all existing presentation deadlines. Measure real displayed full frame.


## Notes

**2026-10-24T10:06:42Z**

RED graphical test compiles and now paints before each of90 frames, with hide/show and recreation. Existing15-second watchdog fails before completing first cycle. Brush path still calls queue-wide idle; testing fence-scoped completion next, preserving the watchdog.

**2026-10-24T10:28:54Z**

GREEN normal Wayland recreation/hide/show test now1/1 across270 physics-and-brush frames under unchanged15sec watchdog. Each cycle records90 brush fence waits and zero queue-idle calls; kinematic motion, finite acquire, image dependencies and per-image semaphores all remain correct. Startup additionally needed deferred graph pipelines underctlo-6waf. Sync-validated replay pending after structural tests; total500-frame scene remains above30ms.
