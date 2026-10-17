---
id: ct-f83a
status: open
deps: [ct-q6fr]
links: []
created: 2026-10-17T07:51:26Z
type: task
priority: 1
parent: ct-hhmy
tags: [vulkan, rigid, dynamic-capacity]
---
# Implement parallel dynamic body allocation and actual-count work queues

## Acceptance Criteria

GPU-parallel free/live slot scans, generation-safe IDs, slab/page allocation and indirect counts; host grows real Vulkan resources only at safe transaction boundaries. No fixed compiled body ceiling or full-max work scans. Memory failure preserves pending sources and IDs and retries.


## Notes

**2026-10-17T08:51:05Z**

CPU-only preparatory RED→GREEN3/3 for resource-boundary hierarchical scan planning: no fixed page limit, partial128-lane groups, recursive levels, 64-bit scratch offsets and 2D dispatch within real device limits. UINT32_MAX entries plan5levels (without allocating that GPU payload). This is dispatch/allocation metadata only, not a CPU body scan/solve. Runtime allocator kernels are not implemented yet; ct-q6fr full regression is still running before dependency closure.

**2026-10-17T08:52:46Z**

CPU recording RED→GREEN2/2 prepares hierarchical scan dispatches: ascending block scans followed by descending parent-prefix additions; each pass has compute dependencies and 24byte uvec2-address push constants. Short scratch/alignment/address-overflow checks prevent any GPU commands. This records O(log(page_count)) dispatch metadata only; page/body values are never scanned on CPU. GPU scan shader/test RED remains next; fresh full runtime regression continues independently without source/SPIR-V changes to its kernels.

**2026-10-17T08:54:36Z**

GPU prefix RED observed expected wrong-prefix/totals with no-op shader; minimal GREEN parallel128-lane scan + descending parent addition now exact for1,127,128,129,16384,16385 entries (up to3hierarchy levels), zero mismatches/guard corruption/VUID/SYNC under six1second fences. No serial whole-list scan: each group scans128 pairs in7barrier steps, global dependencies ascend/descend O(log_128 N) passes. Full runtime make92479 paused only after pixel_world8/8 child completed, then resumed immediately after bounded GREEN. Actual allocator reservation/commit remains pending.
