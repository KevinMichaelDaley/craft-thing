---
id: ctr-8y4b
status: open
deps: [ctr-9svu]
links: []
created: 2026-10-14T03:12:55Z
type: task
priority: 1
parent: ctr-6fii
tags: [renderer, vulkan, appearance, lighting]
---
# Generate coherent textured wood, stone and brick/mortar appearance

Populate the separate color channel using coherent deterministic grain/rings, stone color/relief and brick/mortar wear styles; add optional normals/roughness and imported texture/override path. Physical pixels remain authoritative; cosmetic grout is documented as cosmetic unless its owner really has another physical material.

## Acceptance Criteria

Visible multi-color/relief variation; exact pixel/material mapping; stable seeds and rest-space appearance under rotation/camera changes; no cross-material bleeding, fake filled holes or normal-frame CPU physics copyback.

