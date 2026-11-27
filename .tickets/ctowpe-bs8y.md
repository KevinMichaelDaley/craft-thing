---
id: ctowpe-bs8y
status: in_progress
deps: []
links: []
created: 2026-11-27T10:57:05Z
type: task
priority: 1
assignee: kmd
parent: ctowt-7388
tags: [offscreen, streaming, world-clock]
---
# Schedule reopened resident regions with different confirmed times during a camera water handoff

## Design

On repeated camera pans the opposite edge of the resident atlas can reenter with an older confirmed time. Preserve per-owner history and all water, marker, pigment, heat and velocity channels. Restore a coherent transport interval for the hydraulically connected participants and maintain independent solver work for any separate region. Preserve rejected ticks for retry. Never advance a stored timestamp without elapsed physics or the established idle proof.

## Acceptance Criteria

After spending simulation time in both camera positions, a restored far-edge tile with an older clock cannot freeze the open basin. Repeat the pan and compare connected stock and carried fields against a monolithic run. An actually stale participating generation must still reject the whole interval with both clocks unchanged.

