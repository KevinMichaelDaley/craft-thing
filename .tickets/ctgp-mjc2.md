---
id: ctgp-mjc2
status: open
deps: []
links: [ctsg-6060]
created: 2026-11-26T19:10:19Z
type: bug
priority: 1
assignee: kmd
parent: ctlo-r2ap
tags: [gas, vulkan, publication]
---
# Restore fused practical gas publication with a standalone heat stage

The native accepted-publication fixture records three submissions versus its one-submission contract on both 88965f72 and the corrected first-axis path. The test stops at that assertion before checking rejection. Standalone conduction and pressure feedback are included in the observed workflow.

## Design

Trace command batches through conduction, preparation, accepted pressure work and feedback. Preserve temperature refresh, component ownership and recoverability on rejection when reducing completion boundaries.

## Acceptance Criteria

Keep the existing one-submission check for the supported fused route and verify exact accepted and rejected state, clock and policy transitions. Cover standalone heat diffusion as a separate physically accounted path.


## Notes

**2026-11-26T19:23:32Z**

The exact 9513f4f3 Vulkan publication run also reported 40/48 submissions with exact packets for full fused ticks, versus 8/16 expected. The fused geometry comparison fails the same submission gate. Trace the geometry and full-tick count separately while preserving the matching packets.
