---
id: ct-l35t
status: in_progress
deps: []
links: []
created: 2026-10-05T04:05:20Z
type: task
priority: 2
parent: dun-5kye
tags: [gpu, rigid, geometry]
---
# Store and rasterize convex rigid shapes on Vulkan

Prerequisite for dun-ci2x: represent pre-rotated convex polygons in body-local fixed-point coordinates, with stone/wood material, in the existing bounded GPU body pool.

## Design

Preserve box API and conservative swept AABB broadphase. Rasterize polygon-versus-one-cell overlap on Vulkan, including edge cells. Persist shape records with backwards-compatible box snapshot loading. Contact generation and solving remain in dun-ci2x and dun-9qub.

## Acceptance Criteria

Rotated stone/wood polygons rasterize exact occupied cells across chunk seams, reject malformed or non-convex geometry without mutation, retain conservative broadphase candidates, survive camera rebases and snapshots, and clear shapes on slot reuse.

