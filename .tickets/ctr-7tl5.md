---
id: ctr-7tl5
status: open
deps: [ctr-8y4b, ctr-pbtf]
links: []
created: 2026-10-14T03:12:55Z
type: task
priority: 1
parent: ctr-6fii
tags: [renderer, vulkan, appearance, lighting]
---
# Compose lit textured surfaces and add renderer controls

Replace normal-mode flat colors with appearance plus directional irradiance/normal response, linear HDR shading and one display conversion. Retain within-cell coverage, water/granular composition, unlit reference mode and exact diagnostic overlays. Add usable light/sun/material preview controls.

## Acceptance Criteria

Normal renderer visibly uses textures and lights; lighting toggles do not alter physics. Old exact-color tests explicitly use unlit diagnostics. Four-plus paused frames are stable with no shimmer; edge coverage, wet material and normal rotation are validated.

