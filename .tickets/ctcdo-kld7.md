---
id: ctcdo-kld7
status: closed
deps: [ctdot-kbys]
links: []
created: 2026-11-27T04:17:01Z
type: task
priority: 1
assignee: kmd
parent: ctoew-5swd
tags: [offscreen, streaming, structural]
---
# Transfer dynamic bodies and structural bonds between camera and offscreen owners

Camera integration currently gates automatic workspaces to stationary isolated chunks. A body or support graph spanning the boundary remains resident-owned until an explicit handoff exists.

## Design

Transfer body and member identities, contact state, transforms, momentum and external bond edges at a confirmed epoch. Acknowledge the owning generation before either solver resumes and keep cold recovery atomic for the entire transferred group.

## Acceptance Criteria

A moving body and a spanning bonded assembly cross the camera boundary in both directions with one authoritative owner throughout. Verify time, momentum, bonds, failure retry and cold reopen on the RTX native path.

## Notes

**2026-11-27T12:57:15Z**

Native RTX tests now cover a two-page bonded camera pan, remapped runtime slots, member generations, terrain contact records, a failed page return followed by shutdown, and cold reopen. The current transfer accepts one connected converted authored graph. Combined Vulkan validation and the full regression suite are still running; mixed authored graphs and uninterrupted momentum comparison remain to verify.

**2026-11-27T13:00:02Z**

The floor-contact camera case preserves the complete world-space terrain contact records. Bonded member position, velocity, mass, inertia, angle and angular velocity also match an independently authored uninterrupted native solver after a fractional tick before eviction and a timed tick offscreen. Both camera paths passed, including cold recovery after a failed return.

**2026-11-27T13:18:57Z**

Mixed authored component graphs retain resident joints, member generations and clocks while a bonded pair transfers, returns and reopens. Eight native contact, camera, failure/retry and partition scenarios passed with Vulkan synchronization validation on persistent Xephyr :93.
