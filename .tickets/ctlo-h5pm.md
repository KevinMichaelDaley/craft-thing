---
id: ctlo-h5pm
status: closed
deps: []
links: []
created: 2026-10-24T11:04:31Z
type: task
priority: 1
---
# Distribute contact work across smaller Vulkan workgroups

## Acceptance Criteria

Compiled SPIR-V must use the selected 16-lane workgroup (32 lanes was the measured intermediate), retain all contact work coverage, strict canonical contacts against frozen 64-lane shader; actual displayed repeated brush benchmark below prior cost. No geometry, substep, capacity, or frame-time threshold reduction.


## Notes

**2026-10-24T11:08:54Z**

32-lane GREEN compiled reflection and actual core+synchronization validation canonical manifold oracle 7/7 exact (rotated solid, hollow, comb vs frozen 64-lane reference). Real displayed 500-frame replay:30.283ms mean37.502ms worst vs31.545/37.961 immediately before; four paused captures identical. Improvement is insufficient; testing subgroup-sized16 lanes next before choosing default.

**2026-10-24T11:23:03Z**

Continue workgroup optimization to16 lanes (one native subgroup) after32-lane gain. Compiled reflection RED still32; keep full64-lane scalar reference and verify exact contact coverage before selecting16 as default.

**2026-10-24T11:35:44Z**

16-lane GREEN reflection and actual core+synchronization canonical manifold oracle7/7 exact. Displayed API-only500-frame diagnostic27.702ms mean37.679ms worst with existing combined options (p952? actual95th31.467ms), no mid-frame allocations or shader compiles. Long-scan/page-cache fluid options did not improve full-frame outcome:28.133/35.885; keep those experiments off. Global frame target remains unmet.

**2026-10-24T19:30:38Z**

Selected16 lanes after intermediate32-lane measurement; acceptance text now reflects selected configuration. Full final regression retains compiled reflection, exact rotated solid/hollow/comb manifolds against frozen64-lane oracle, selector10/10, and bounded strided work coverage. Combined default displayed1000-frame replays24.079/29.334ms and24.133/29.147ms mean/worst; no physics/cadence/capacity reduction.
