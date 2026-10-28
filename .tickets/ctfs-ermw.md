---
id: ctfs-ermw
status: open
deps: []
links: []
created: 2026-10-28T03:33:23Z
type: feature
priority: 1
parent: ctlo-6syy
tags: [siege, fire, materials]
---
# Admit finite coal and char projectile materials as actual rigid geometry

Extend the shared terrain/body material mapping, density, extraction and profile generation for coal and char. The siege projectile must use its own material density/fuel/thermal stock, not wood painted black.

## Acceptance Criteria

RED-GREEN-REFACTOR. Native GPU coal/char bodies preserve actual material through admission, mass/inertia, collision, rendering, burning, and snapshot restore. A cold coal control stays cold; the finite hot shot emits state-driven heat/flame and leaves finite char/ash. Shared C/GLSL mapping round trips correctly without extending physics resolution or introducing fixed body counts.

