---
id: ctlo-33fv
status: in_progress
deps: []
links: []
created: 2026-10-24T08:58:08Z
type: task
priority: 1
---
# Pack carved pixel tiles in parallel with stable GPU prefixes

## Acceptance Criteria

RED probe exposes hundreds of tile packing barriers; GREEN bounds packing synchronization independently of tile count. Keep source tile ordering, sparse directories, fragments, materials, colors and exact mass. Preserve dynamic growth, whole brush regression battery and displayed frame budget.


## Notes

**2026-10-24T09:02:43Z**

RED2/2 packing-work failures:288 per-tile barriers. GREEN2/2 actual core/synchronization checks:16 stable-prefix packing barriers, one source read per packed material word, exact36051 remaining pixels/material/customRGBA/mass and rotated misses. Displayed benchmark running; full brush regressions next.

**2026-10-24T09:17:15Z**

Normal full brush suite12/12 after tile packing, large190x190 cut9.791ms versus11.296ms before packing. Displayed500-frame result31.148ms mean36.126ms worst; still over budget. Packed-commit and shared-gather diagnostic controls show no decisive MPM frame win; remain optional.
