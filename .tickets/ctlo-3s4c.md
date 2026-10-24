---
id: ctlo-3s4c
status: in_progress
deps: []
links: []
created: 2026-10-24T11:35:09Z
type: task
priority: 1
---
# Cache immutable rigid pixel boundaries once per solver batch

## Acceptance Criteria

GPU-parallel packed-word boundary cache, refreshed after cuts/extraction and for standalone contact queries. Preserve every geometry/material/color byte and exact canonical manifolds for rotated concave bodies; validate cache growth and shader synchronization. No body limit, no CPU scan, and retain full scene memory budgets.


## Notes

**2026-10-24T11:50:32Z**

Packed cache exact checks pass: every mask in512-word solid/comb and384-word hollow shapes plus canonical manifold oracle7/7 under core+synchronization validation. Geometry masks are scratch only, source shapes and color maps remain untouched. Full displayed500-frame replay28.267ms mean34.812ms worst with combined options, so no decisive gain yet. Investigating duplicate post-solver cache refresh; validating automatic geometry growth now.

**2026-10-24T11:51:35Z**

Automatic large-component geometry growth passes actual core+synchronization validation, retaining382x254 stone pixels and mass after arena expansion. Body-carve suite12/12 passes with cache enabled, including custom RGBA, concavities, repeated splitting and leaves. Shader interface tests9/9 pass with ABI4, which rejects pre-cache modules; generated shader dependencies now track the versioned header.
