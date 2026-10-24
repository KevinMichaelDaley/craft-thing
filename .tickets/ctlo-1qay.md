---
id: ctlo-1qay
status: in_progress
deps: []
links: []
created: 2026-10-24T19:10:51Z
type: task
priority: 1
---
# Coalesce staged rigid color copies across GPU lanes

## Acceptance Criteria

Preserve all RGBA bytes in both geometry banks and zero-fill records; use eight cooperative lanes per eight-texel word without marker overwrite races; retain serial fallback; prove coverage under Vulkan synchronization validation and measure actual displayed repeated-cut frame time.

