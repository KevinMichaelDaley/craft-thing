---
id: ctols-vhg9
status: open
deps: [ctols-xhuy]
links: []
created: 2026-11-27T05:12:47Z
type: task
priority: 1
assignee: kmd
parent: ctcdo-rcs9
tags: [offscreen, streaming, world-clock]
---
# Bind liquid face commits to paired streaming owners and epochs

The Vulkan liquid operator commits a whole face or reports a rejected or unconfirmed submission. The level and offscreen owner routes still need to bind this work to their admitted generations and confirmed world intervals.

## Design

Reserve adjacent mapped owners for one confirmed interval. Publish acknowledgement and both updates once. Hold mismatched or missing peers at the boundary. Recover both owners from confirmed state if acknowledgement was lost, and keep cold publication ordered with seam work.

## Acceptance Criteria

Tests use real level and offscreen ownership to reject a stale generation, mismatched time and duplicate interval. Inject a lost acknowledgement after GPU work and verify recovery and cold reload preserve both exact stocks. Keep the open seam parent active for pressure-derived plans, gas and grain coupling.


## Notes

**2026-11-27T05:12:47Z**

The GPU operator takes world coordinates and mapped slots, validates all lanes before writing either side, and distinguishes a proven rejection from an unconfirmed submission. The paired caller owns the generations, pressure interval and recovery.

**2026-11-27T05:16:24Z**

The current world save transaction can group durable records under one draft directory. dc_stream_stage binds a worker to one staging directory, so periodic paired publication will need an explicit rebind or a pair journal. Keep the saved pair recoverable at its common epoch before widening camera eligibility.
