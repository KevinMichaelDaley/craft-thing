---
id: ctolt-6it2
status: in_progress
deps: []
links: []
created: 2026-11-27T00:35:15Z
type: bug
priority: 1
assignee: kmd
parent: ctfm-w80b
tags: [world-clock, streaming]
---
# Preserve ordered durations of queued level simulation steps

The live level adds queued step durations then divides by the count. Unequal accepted requests therefore execute as artificial equal-length ticks, which changes nonlinear simulation and step-boundary input. An invalid request can also be masked by averaging.

## Acceptance Criteria

Preserve the order and exact requested time of unequal, valid ticks, reject invalid intervals at enqueue, and retain the existing single-frame batch for equal intervals. Verify behavior through the production level queue and render entry points, including retry after partial mixed progress and the gameplay loading cadence.

