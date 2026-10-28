---
id: ctfs-0zw5
status: in_progress
deps: []
links: []
created: 2026-10-28T03:33:23Z
type: feature
priority: 1
parent: ctlo-6syy
tags: [siege, authoring, physics]
---
# Admit touching authored rigid objects without merging their ownership

Cold programmatic rigid admission must preserve explicit per-pixel object IDs even where two objects or an object and terrain touch. Reuse parallel component extraction and dynamically growing owner/geometry resources; preserve colors/reactive parcels. This is the narrow siege fixture prerequisite, not the remaining all-role atomic prop publication scope.

## Acceptance Criteria

RED-GREEN-REFACTOR. Two touching wood components tagged as distinct objects become distinct real rigid bodies; a concave arm/pouch stays one connected body. Chunk-seam adjacency honors object IDs. Terrain and loose untagged material retain their own ownership. Preserve transaction/object metadata through body storage growth and checkpoints. GPU derives connectivity; no host component scan or per-frame CPU solve. Cold staging is released safely and ordinary post-admission automatic extraction stays unchanged.

