---
id: ctr-1fpw
status: closed
deps: []
links: [ctr-pqve]
created: 2026-10-17T19:33:47Z
type: feature
priority: 1
---
# Carve rigid and structural bodies with the air brush

## Acceptance Criteria

Air brush removes intersected moving-body material in world space; disconnected remnants become independent bodies, full erasure removes body, retained pixels preserve material/color and velocity, updated mass/collisions/save state match geometry. Terrain/water/grain erasing remains intact. Cover rotated and compound bodies.


## Notes

**2026-10-17T19:43:16Z**

GPU air-brush transaction now covers sparse pixel bodies, rotated legacy convex/compound bodies (rasterized at physical pixel resolution), and structural members. Hit masks and RGB transfers are parallel; existing fracture grouping/reservation publishes disconnected remnants and preserves pose/velocity/material/RGB. Fully erased bodies retire; carved structural members retire from support graph. Four focused tests pass under core/sync validation, including capacity rollback and carved-body save/load. Full suite running.

**2026-10-17T19:48:48Z**

Final focused 4/4 pass with Vulkan core and synchronization validation, including clearing old body bonds and roundtripping a carved structural member. Remaining shared fracture/remnant grouping optimization is explicitly tracked by ctr-pqve; this change adds no ordinary-tick carving dispatch.

**2026-10-17T19:51:49Z**

Final make -j4 -i test completed, including body_carve_tests 4/4: only the pre-existing authored_stage_tests component-plan getter and absent-normal override REDs (ctr-ojvy), no new failures. Final focused core/sync validation clean. Demo rebuilt; resume previous world directory to preserve edits. Shared remnant parallelization follow-up: ctr-pqve.
