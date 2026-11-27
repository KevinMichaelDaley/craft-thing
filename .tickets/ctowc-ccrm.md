---
id: ctowc-ccrm
status: closed
deps: [ctowd-09br]
links: []
created: 2026-11-27T08:02:26Z
type: task
priority: 1
assignee: kmd
parent: ctowt-7388
tags: [offscreen, streaming, physics]
---
# Advance a shared water domain on the native phase cadence

## Design

Use the resident water phase schedule for the connected domain. Retain pressure and transport intermediates across confirmed ticks. Make phase writes visible to the subsequent native physics stages of both owners. Keep the owner set pinned throughout the cycle so a remap cannot invalidate an unfinished interval.

## Acceptance Criteria

A split basin reproduces a monolithic native simulation over repeated water cycles with intervals 1, 2, 3 and 6 and mixed elapsed steps. Check intermediate stock and phase state, recover a lost acknowledgement exactly once, and preserve both owner clocks when an interval cannot be confirmed.

## Notes

**2026-11-27T08:11:59Z**

The ordinary tick recorder uses a prospective phase range, scale, and remaining budget. Five schedule checks cover every supported cadence, mixed elapsed steps, and float boundaries. Native recorder tests verify water runs after rigid work and before bulk, particles, and thermal work, including dry cycle advancement. Next, retain the shared pressure and transport state between these phase ranges and publish each confirmed range to both owners before their native post-water stages.

**2026-11-27T09:56:18Z**

Verified split and monolithic water state for intervals 1, 2, 3, and 6, including partial-phase retry and receipt recovery. Page remaps and chunk replacement are rejected while owners are held. The level scheduler and staged save integration continue under ctowt-7388.
