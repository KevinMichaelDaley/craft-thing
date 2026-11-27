---
id: ctowt-7388
status: in_progress
deps: [ctowt-o11u, ctowp-v2rf, ctowtb-yr4g, ctowtb-kz7x, ctowd-09br, ctowc-ccrm, ctowpe-bs8y, ctowpe-fbhn, ctowpe-uoaa]
links: []
created: 2026-11-27T06:30:47Z
type: task
priority: 1
assignee: kmd
parent: ctopp-2c14
tags: [offscreen, streaming, physics]
---
# Apply the full water sweep through the paired owner transaction

## Design

Bind both current generations at the start of the same transport interval. Validate and atomically apply the native paired water write set and carried fields with an explicit receipt and durable recovery. An owner participates once in a transport interval, including domains touching more than one seam.

## Acceptance Criteria

The live split and monolithic water basins conserve stock and the carried state across repeated camera admission. A stale generation, rejected batch, or missing receipt cannot publish a partial write set or advance only one owner.


## Notes

**2026-11-27T07:05:09Z**

The atomic GPU publication operation is available after settled rigid displacement. The level still needs to reserve the connected owner cohort at one interval start, provide the correct native pre-transport and post-transport phases, and publish its completed timestamps through the staged save. The existing explicit face-transfer entry point binds owners at a completed interval and cannot substitute for this schedule.

**2026-11-27T07:36:58Z**

Paired publication now includes native marker advection, correction and velocity damping. The detached cycle leaves both context clocks and their active bank selectors with their owners. Rigid displacement is tracked by ctowtb-kz7x; generation reservation and interval scheduling for the connected set of owners remain part of this ticket.

**2026-11-27T07:47:40Z**

The sealed two-tile water cycle now begins with native rigid displacement using current occupancy and ends with native marker correction and damping. ctowd-09br tracks the shared solve and atomic receipt for a connected set of mapped owners. The level still needs to schedule that cycle once at a common interval, advance both owner clocks after confirmation, and publish the resulting snapshots through the staged world save.

**2026-11-27T08:12:55Z**

One scratch domain now covers the connected mapped tiles of the resident context and a shared-device workspace. One retained receipt confirms all supplied tiles, including a tile that touches multiple seams. Live scheduling of the partial native water phases and durable camera admission remain open. ctowc-ccrm tracks the shared phase cadence.

**2026-11-27T10:04:23Z**

The workspace now defers independent ticks and preserves its lease, mappings, and clock through partial water cycles. A native two-context regression verifies that admission, cancellation and snapshot publication resume after the final phase. The level still needs to schedule both owners from a common interval start, run pre-water and post-water physics, and confirm both timestamps across camera admission.

**2026-11-27T10:08:08Z**

New workspaces inherit the resident cadence and marker correction setting. Level settings update all leased workspaces together and preserve phase state if any held owner rejects a change. The level tick suite passed 22 tests and the native offscreen workspace suite passed 3.

**2026-11-27T10:32:09Z**

The level tick path now binds adjacent admitted structure-workspace tiles to the same elapsed interval as all mapped resident tiles. Both owners run source and rigid work before shared water and bulk, particles, thermal and gas afterward. The displayed Vulkan fixture checks two full cycles against a monolithic basin, verifies color and stock after reopen, rejects stale generations before consuming the queued tick, and restores the durable pair when a receipt cannot be recovered. Camera disk election still selects isolated islands. Open-basin camera election and return during a partial interval remain unverified.

**2026-11-27T10:44:34Z**

Two camera evictions and returns now match an independent monolithic simulation for per-cell water mass, face velocity and pigment. The test pans back during a partial phase with an elapsed tick queued. A dry returning tile is eligible when its connected resident neighbor holds water. Camera admission with a failed workspace load and an eviction initiated midway through an ordinary resident cycle still need coverage.

**2026-11-27T10:58:36Z**

Verified eviction halfway through resident pressure work on Xephyr :93 against a monolithic GPU solve. Repeated pans exposing a region with an older confirmed clock reject atomically and retain the elapsed tick. Tracked recovery in ctowpe-bs8y.
