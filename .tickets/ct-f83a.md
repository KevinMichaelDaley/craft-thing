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

