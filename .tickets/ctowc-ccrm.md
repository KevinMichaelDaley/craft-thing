---
id: ctowc-ccrm
status: in_progress
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
