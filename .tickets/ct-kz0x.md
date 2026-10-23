---
id: ct-kz0x
status: open
deps: [ct-f83a]
links: []
created: 2026-10-17T07:51:26Z
type: task
priority: 1
parent: ct-hhmy
tags: [vulkan, rigid, dynamic-capacity]
---
# Migrate all rigid physics and rendering consumers to dynamic body storage

## Acceptance Criteria

Broadphase, contacts, solver, hydro, raster, draw, geometry/properties, diagnostics and structural ownership consume runtime body records/counts. Preserve exact shape/material data and current performance gates; structural graph limits remain independent.

