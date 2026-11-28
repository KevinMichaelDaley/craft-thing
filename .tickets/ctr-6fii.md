---
id: ctr-6fii
status: in_progress
deps: []
links: [ctlo-h9of]
created: 2026-10-14T03:12:55Z
type: epic
priority: 1
tags: [renderer, vulkan, appearance, lighting]
---
# Textured material pixels and directional sun/point lighting

Implement design/textured_directional_renderer.md in isolated feat/material-light-renderer worktree. Physics material pixels remain authoritative; separate appearance/color and directional lighting channels, coherent textures, Noita-style occlusion, preserved moving-body appearance, and measured Vulkan rendering.

## Acceptance Criteria

All staged tickets pass RED-GREEN-REFACTOR, exact geometry/physics invariance, persistence, stable paused visual tests, Vulkan/SPIR-V validation and measured quarter-native renderer budgets. Integrate against ct-wuci only after coordinated merged validation.


## Notes

**2026-11-28T04:54:07Z**

Xephyr displayed the rotating concave, textured, four-light/sun fixture at 480x270 for 90 frames in 0.907s with exact masses and four identical paused captures. The separate 24-sample performance gates failed: 480x270 mean 15.993ms, worst 62.538ms; 640x360 mean 15.301ms, worst 37.314ms. Other tests shared the GPU during these measurements. The strict worst-frame gates remain open and need an isolated repeat.
