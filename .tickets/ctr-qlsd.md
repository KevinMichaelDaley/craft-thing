---
id: ctr-qlsd
status: open
deps: [ctr-21eh, ct-wuci]
links: []
created: 2026-10-14T03:12:56Z
type: task
priority: 1
parent: ctr-6fii
tags: [renderer, vulkan, appearance, lighting]
---
# Integrate renderer branch with completed parallel extraction

Rebase/merge completed ct-wuci into isolated renderer branch, resolve API/shader/storage changes cooperatively, run complete combined validation, then integrate verified renderer changes to main with parent coordination.

## Acceptance Criteria

No overwritten extraction optimizations or stale serial code; full combined correctness/visual/performance results, ticket statuses and clean pushed branches/main. Explain any still-open dense-fluid ct-irzt limitation; never infer whole-frame60FPS from renderer-only cost.

