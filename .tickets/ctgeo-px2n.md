---
id: ctgeo-px2n
status: in_progress
deps: []
links: [ctgc-tteu]
created: 2026-11-26T19:16:08Z
type: bug
priority: 1
assignee: kmd
tags: [gas, vulkan]
---
# Use physical cell order for explicit gas sweeps

Fused hybrid shaders invoke the explicit stage before assigning the private implicit scan direction. Explicit shared line storage is indexed by physical coordinates, so it must not depend on the implicit pass or direction bank bit.

## Acceptance Criteria

All standalone and fused explicit shader paths traverse line cells in physical order regardless of implicit direction. Compiled shader regression covers generic, fixed-axis and local-stage paths. Existing native transport comparisons pass.

