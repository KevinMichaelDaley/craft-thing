---
id: ctowpe-uoaa
status: closed
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


## Notes

**2026-11-27T12:17:48Z**

All five partial phases now reopen with no queued elapsed time, match the first resumed native tick and finish with matching stocks, velocities, markers and pigment. With the first resumed receipt and its recovery both lost, close refuses publication and the prior partial generation reopens successfully. The public constructor camera test also checks heat and bulk momentum.

**2026-11-27T12:19:10Z**

The camera restart case passed Vulkan synchronization validation on Xephyr :93 via dc_level_view_create, including re-election of the saved owner, continued heated bulk flow and subsequent camera returns. The five-phase restart and lost-receipt run exited successfully.
