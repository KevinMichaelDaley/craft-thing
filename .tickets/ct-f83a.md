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
