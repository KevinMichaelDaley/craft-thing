---
id: ctols-vhg9
status: closed
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

**2026-11-27T05:40:45Z**

Two RTX Vulkan regressions passed with core and synchronization validation. A mapped resident and an admitted offscreen lease at the same completed interval rejected stale generations, a moved mapping, unequal epochs, and repeat application. A real commit with a lost acknowledgement recovered its retained GPU receipt exactly once. An obstructed staged worker write preserved the canonical epoch. Retrying and closing published 11,000 water units on each side, and fresh generations could not replay the interval after cold reopen. A second injected failure made the receipt unreadable, fenced simulation and publication, and left both canonical stocks at 16,000 and 6,000 after reopen. The world draft abort and restage check passed under ASan/UBSan. The existing world transaction stages paired edits through orderly shutdown.
