---
id: ctr-pqve
status: open
deps: []
links: [ctr-1fpw]
created: 2026-10-17T19:48:43Z
type: task
priority: 2
---
# Parallelize sparse fracture and brush remnant graph construction

Brush hit classification and RGB transfer are parallel, but remnant grouping and geometry construction reuse the pre-existing single-invocation pixel fracture BFS/transaction. Replace that shared stage with parallel GPU connectivity, reservations and emission for large components; preserve transactional failure, per-pixel RGB/material, angular velocity and structural bond retirement.

## Acceptance Criteria

No single-invocation full-component BFS or geometry copy in impact fracture or air-brush remnant construction; benchmark large concave cuts on this Vulkan machine, preserve existing body_carve/body_color/fracture regression results.

