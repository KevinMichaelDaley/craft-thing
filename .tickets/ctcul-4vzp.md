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

**2026-11-26T22:05:44Z**

The compare-and-swap change on 9d712cd0 preserves parent 2 and returns roots 0/0/0 on the native probe. The exact focused colored workload passed both cases with 36,051 pixels and matching mass and color. The body-carve and full suites remain queued at this exact revision.

**2026-11-27T00:13:08Z**

The production shader repro was replayed on the current integration stack. Native RED observed parent 2, then retained parent 1 and roots 0/1/0. The ported compare-and-swap retains parent 2 and produces roots 0/0/0. Both ordinary and validation-enabled NVIDIA runs passed. The current-stack body-geometry, color and admission checks are queued.
