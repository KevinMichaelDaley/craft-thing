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

**2026-11-28T04:54:07Z**

Native Wayland on a GL Weston compositor nested in persistent Xephyr :93 advertised RTX A2000 Wayland presentation. Both real progress paths passed: a failed present retained two completed ticks and the gas acquire timeout retained both physical steps without a duplicate retry. The three-cycle recreation/brush test still exceeded its unchanged 15-second watchdog on this setup. The enclosing make -k test_ui is running. The earlier pixman-only nested compositor could not present from the NVIDIA device.

**2026-11-28T05:03:19Z**

Repeated native Wayland on a fresh, private device cache. All three 90-frame cycles reported exactly 90 brush fence waits, zero queue-idle calls, finite acquire, safe per-image semaphores and both checked transfer dependencies. The child still exceeded the unchanged 15-second aggregate watchdog; the second/third starts were 3.85s apart. The 18.8MB private cache reloaded, so the prior near-limit shared cache alone cannot explain the deadline.

**2026-11-28T06:00:03Z**

Traced duplicate full-sized memset calls during each actual Wayland constructor: water, shape appearance and bulk state each received two zero fills. Removed the redundant client-side fills and checked the constructor directly while running all 90 real brush submissions. Each waited its fence, with zero presentation queue-idle calls. One three-cycle run passed the unchanged deadline, but repeated runs still timed out under concurrent rendering; device shutdown alone reached 3.1 seconds in a trace. Keeping the ticket open.
