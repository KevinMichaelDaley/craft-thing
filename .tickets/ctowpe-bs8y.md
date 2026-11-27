---
id: ctowpe-bs8y
status: closed
deps: []
links: []
created: 2026-11-27T10:57:05Z
type: task
priority: 1
assignee: kmd
parent: ctowt-7388
tags: [offscreen, streaming, world-clock]
---
# Schedule reopened resident regions with different confirmed times during a camera water handoff

## Design

On repeated camera pans the opposite edge of the resident atlas can reenter with an older confirmed time. Preserve per-owner history and all water, marker, pigment, heat and velocity channels. Restore a coherent transport interval for the hydraulically connected participants and maintain independent solver work for any separate region. Preserve rejected ticks for retry. Never advance a stored timestamp without elapsed physics or the established idle proof.

## Acceptance Criteria

After spending simulation time in both camera positions, a restored far-edge tile with an older clock cannot freeze the open basin. Repeat the pan and compare connected stock and carried fields against a monolithic run. An actually stale participating generation must still reject the whole interval with both clocks unchanged.


## Notes

**2026-11-27T11:14:41Z**

On Xephyr :93 the third round trip now matches the monolithic basin cell by cell. A separate older pool advances its own clock and transports its own stock on the same dispatch. An old connected participant and a stale pinned generation both reject the interval before consumption.

**2026-11-27T11:18:15Z**

Both ambient and thermal camera runs passed three round trips. Verified actual marker identity and local coordinates and per-cell pigment, velocity, stock and excess heat against a separate native GPU context. Mixed-clock admission excludes live reactive parcels, active probes, gas, bodies and nonwater bulk.
