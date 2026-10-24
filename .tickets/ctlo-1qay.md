---
id: ctlo-1qay
status: in_progress
deps: []
links: []
created: 2026-10-24T19:10:51Z
type: task
priority: 1
---
# Coalesce staged rigid color copies across GPU lanes

## Acceptance Criteria

Preserve all RGBA bytes in both geometry banks and zero-fill records; use eight cooperative lanes per eight-texel word without marker overwrite races; retain serial fallback; prove coverage under Vulkan synchronization validation and measure actual displayed repeated-cut frame time.


## Notes

**2026-10-24T19:14:47Z**

RED GPU probe preserved3 words but reported0 cooperative copy lanes. GREEN probe now reports24 lanes for24 texels, exact RGBA in both arena directions and zero-fill, still3 pool/3 capacity queries. Actual core+synchronization validation probe passes. Production body-carve range uses padded eight-lane teams with subgroup marker-read barrier; other call sites keep original serial helper. Integration/performance validation underway.

**2026-10-24T19:20:29Z**

Full actual core+synchronization body-carve suite12/12 passes. Default displayed1000 replay24.467ms mean29.815ms worst passes, but repeat24.960/30.562 still misses strict tail budget. Preserve the byte-exact cooperative implementation; global performance work continues.
