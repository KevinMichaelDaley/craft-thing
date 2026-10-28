---
id: ctfs-0zw5
status: closed
deps: []
links: [ctpf-iueo]
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


## Notes

**2026-10-28T04:07:25Z**

Narrow fresh-scene helper is implemented: object-aware specialization only for cold GPU admission; default simulation kernels specialize the object path off. Source transaction/object metadata is retained through DCS and body growth. GPU counter drains all pending component batches and services actual allocation requests; 100 touching singleton objects pass without a 64-body cap. Explicit complete object masks at resident boundaries do not require surrounding terrain visibility. Malformed tags validate before publication. General all-role atomic live placement/capacity rollback remains on linked parent ctpf-iueo; this helper is restricted to disposable fresh-scene construction if a resource failure occurs.

**2026-10-28T04:14:52Z**

Final real-display Vulkan battery passed6/6 (build/authored_objects_final_display.log): touching IDs, invalid-mask rejection, cross-chunk identity, 42-cell concave arm/pouch and touching load, 100-body growth, and explicit resident-boundary object. Cold placement preserves geometry and velocities; parcel state remains bit-identical; DCS retains transaction/object IDs; ordinary extraction reuses released authored slots without stale provenance. Full make -j1 test passed with zero failures (build/authored_objects_all_regression.log).
