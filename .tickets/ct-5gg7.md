---
id: ct-5gg7
status: in_progress
deps: []
links: []
created: 2026-10-31T21:50:41Z
type: task
priority: 1
assignee: kmd
parent: ctfs-81vw
tags: [fire, smoke, display]
---
# Set transported bonfire smoke near 75 percent display alpha

User requests smoke about three-quarter alpha. Tune the existing Beer-Lambert display coefficient against representative soot density without changing gas state or adding a fake screen animation.

## Acceptance Criteria

GPU display regression measures 0.70-0.80 alpha for a representative transported soot cell; fullscreen quarter-native bonfire remains readable and passes dry/wet ignition checks; full make test passes.

