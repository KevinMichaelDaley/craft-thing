---
id: dun-ci2x
status: open
deps: [dun-4ftd]
links: []
created: 2026-09-26T07:43:35Z
type: task
priority: 2
assignee: kmd
parent: dun-5kye
tags: [gpu, rigid, contacts]
---
# Generate convex-to-pixel GPU narrowphase contacts

Create terrain, MPM-material, and body-body contacts from convex pieces at cell resolution.

## Design

Use convex support/edge tests against occupied one-cell terrain and convex pairs from broadphase; emit contact normal, depth, feature IDs, material parameters, and world-space anchors into bounded GPU buffers. Avoid viewport-local coordinates and report overflow.

## Acceptance Criteria

Rotated convex stones and wood contact sloped and stepped terrain without missed one-cell obstacles; body-body contacts are symmetric and reproducible across chunk seams.

