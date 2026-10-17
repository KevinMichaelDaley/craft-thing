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

**2026-10-17T09:05:03Z**

Atomic allocation RED compiled/SPIR-V validated, executed under bounded Vulkan/sync validation and failed expected tentative ready assertion with stub. GREEN candidate now compiles:64-lane/page count/mask/rank, parallel tentative handles, commit-gated owner writes and separate O(1) status/publication dispatches. NOT GPU-verified yet: parent holds merged renderer regression lease. Main fresh full suite92479 paused only after structural_failure13/13 completed. Remaining allocator scope: actual-count indirect zero-work gates, generation-safe release/reuse, live/ID lists and runtime consumer integration. Current gameplay stilllegacy64.

**2026-10-17T09:06:44Z**

Transactional allocator GPU GREEN verified0.328s with explicit synchronization validation, no errors:90tentative handles with commit0 preserve ownership/free masks; commit1 publishes90once, replay is idempotent;40more defer with required_slots132 and no mutation; store growth128→192 retries same queue and publishes130exact generation1handles. GPU lease returned immediately to parent terrain-placement/demo work. Still prototype only: runtime consumers remainlegacy64, release/reuse/indirect/live-ID work and full integration remain open.
