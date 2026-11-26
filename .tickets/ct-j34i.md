---
id: ct-j34i
status: open
deps: []
links: []
created: 2026-11-26T05:42:31Z
type: bug
priority: 1
assignee: kmd
parent: ct-exwr
tags: [player, beaver, world]
---
# Route camera follow and reticles through the active player viewport

The 3811d47f follow block pans the world around player one while split view is active and converts absolute 64-bit chunk positions to floats. Controller targeting uses the full view center instead of each player viewport.

## Acceptance Criteria

Exercise movement, chunk transitions, editor camera controls, split-screen framing and per-player targets near far and negative coordinates in the actual application.

