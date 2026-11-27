---
id: ctowt-7388
status: in_progress
deps: [ctowt-o11u, ctowp-v2rf, ctowtb-yr4g, ctowtb-kz7x]
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
