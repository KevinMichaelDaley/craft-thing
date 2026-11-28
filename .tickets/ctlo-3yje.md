---
id: ctlo-3yje
status: closed
deps: []
links: []
created: 2026-10-24T09:23:45Z
type: task
priority: 1
---
# Share packed pixel boundary reads across contact subgroup lanes

## Acceptance Criteria

GPU RED proves scalar-equivalent read count. Cooperative ballot/broadcast batches identical body/word queries with exact sparse and concave boundary decisions, fewer geometry reads, and partial subgroup correctness. Preserve per-pixel parallel contact emission and existing work scheduling; validate contacts and displayed frame time.


## Notes

**2026-10-24T09:52:44Z**

GREEN primitive checks4 patterns including partial subgroups:1704 decisions each, zero mismatches; dense9854 to4144 geometry reads. Canonical full128-byte contact digests match scalar reference for rotated solid, hollow and comb64x64 bodies (7/7 checks), actual core/synchronization validation clean. Porous-comb timing regressed with unconditional sharing, so production use now requires more than75 percent occupied tile pixels; sparse cases retain scalar path. Density-gated displayed recheck next; previous ungated500-frame result30.835ms mean47.436ms worst did not meet target.

**2026-11-28T05:14:58Z**

RTX A2000 recheck under Vulkan core and synchronization validation: all 1,704 decisions in each of four full/partial subgroup patterns matched the scalar path. Dense reads fell from 9,854 to 4,144. Fresh canonical manifold runs with the packed cache explicitly disabled and enabled each passed 7/7, covering rotated solid, hollow and comb shapes. The shipping density gate is enabled. A displayed 480x270 run with two rotating concave textured bodies recorded 90 physics/render frames in 0.879 s and four identical paused frames. This measures the production scene; the isolated read reduction is from the counter fixture.
