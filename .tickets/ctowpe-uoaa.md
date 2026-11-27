---
id: ctowpe-uoaa
status: open
deps: []
links: []
created: 2026-11-27T11:20:44Z
type: task
priority: 1
assignee: kmd
parent: ctowt-7388
tags: [streaming, offscreen, save]
---
# Persist a confirmed partial shared pressure cycle with no elapsed time queued

## Design

Retain the scratch pressure, head and previous-stock fields, ordered ownership, cadence, and continuation metadata required to resume a confirmed partial native cycle after restart. Stage the checkpoint with the chunk owners and world clock, and reject unconfirmed writes.

## Acceptance Criteria

After any confirmed partial phase, quit without queuing more time, reopen, then advance one elapsed interval. Match uninterrupted stock, velocity, marker, pigment, energy and clock for both owners. A lost receipt must still expose the previous durable generation.

