---
id: ctlo-3yje
status: in_progress
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
