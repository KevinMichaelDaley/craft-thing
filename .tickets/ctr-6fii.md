---
id: ctr-6fii
status: in_progress
deps: []
links: []
created: 2026-10-14T03:12:55Z
type: epic
priority: 1
tags: [renderer, vulkan, appearance, lighting]
---
# Textured material pixels and directional sun/point lighting

Implement design/textured_directional_renderer.md in isolated feat/material-light-renderer worktree. Physics material pixels remain authoritative; separate appearance/color and directional lighting channels, coherent textures, Noita-style occlusion, preserved moving-body appearance, and measured Vulkan rendering.

## Acceptance Criteria

All staged tickets pass RED-GREEN-REFACTOR, exact geometry/physics invariance, persistence, stable paused visual tests, Vulkan/SPIR-V validation and measured quarter-native renderer budgets. Integrate against ct-wuci only after coordinated merged validation.

