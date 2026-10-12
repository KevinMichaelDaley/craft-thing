---
id: dun-75oh
status: in_progress
deps: [dun-08qq, dun-wc20]
links: [ct-gf6q]
created: 2026-09-26T07:43:18Z
type: task
priority: 2
assignee: kmd
parent: dun-9det
tags: [gpu, rigid, mpm, fracture]
---
# Fracture stone after hard impacts

Stone rigid bodies breaking on sufficiently hard impacts spawn gravel or smaller stone chunks.

## Design

Use contact impulse or XPBD constraint work with a material toughness threshold; execute a bounded GPU fracture transaction that retires the parent, creates child bodies/MPM particles, and carries mass and momentum within documented tolerance. Handle pool saturation explicitly.

## Acceptance Criteria

A gentle drop remains intact; a hard drop fractures into visible pieces, with deterministic counts and mass/momentum budget, no duplicate fragments, and chunk-safe ownership.


## Notes

**2026-10-12T16:09:06Z**

Implementing the shared GPU fragment transaction for ct-gf6q. RED gentle/hard-contact regressions compile and fail. Water/extraction dependencies remain open; ticket will not close before complete chunk-safe visual/conservation acceptance.
