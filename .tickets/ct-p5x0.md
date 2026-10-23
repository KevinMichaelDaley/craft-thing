---
id: ct-p5x0
status: open
deps: [ct-6u7r]
links: []
created: 2026-10-17T07:51:26Z
type: task
priority: 1
parent: ct-hhmy
tags: [vulkan, rigid, dynamic-capacity]
---
# Migrate rigid checkpoints and ownership through dynamic capacity changes

## Acceptance Criteria

Read old64body snapshots without losing any source, allow arbitrary counted records with validated bounds/checksum, canonical geometry and generation-safe identity; preserve allocator/world/chunk relationships and rollback corrupt or allocation-failed loads.

