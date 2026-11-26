---
id: ctcul-4vzp
status: in_progress
deps: []
links: [ctlo-jnt7]
created: 2026-11-26T21:53:29Z
type: bug
priority: 1
assignee: kmd
tags: [vulkan, rigid, carve]
---
# Preserve concurrent connectivity links during rigid pixel carving

The carve union rebuilds a parent after an atomic minimum on a root selected earlier. A stale link can replace a link published by another lane, losing the earlier component edge and permitting an unintended split.

## Acceptance Criteria

Reproduce the stale root interleaving in the real Vulkan link code, retain all accepted edges and stable minimum representative ordering, and verify large colored and split carve fixtures.


## Notes

**2026-11-26T21:54:54Z**

The compiled native Vulkan probe returns observed parent 2 and retains parent 1 on the stale atomic minimum. After retry the roots are 0/1/0, so the already accepted edge is lost. This reproduces the interleaving without relying on scheduling or timing.
