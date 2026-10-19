---
id: ctr-8y4b
status: in_progress
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


## Notes

**2026-10-14T03:20:50Z**

Preparing deterministic texture reference tests while ctr-9svu full-suite validation awaits fair shared-GPU window. Implementation generation is per texel, no ordinary-frame CPU appearance copyback.

**2026-10-14T04:28:32Z**

Parallel GPU texture reference and actual scene-frame GREEN passed with Vulkan validation. Separate color cache is world-indexed and generation/material protected; coherent grain is now used by frame composition. Explicit imported/source-owned body appearance and persistence remain ctr-ddtf; displayed acceptance/performance still pending.

**2026-10-14T04:48:20Z**

Pushed fb35ce8 RED and 0e2015b GREEN: exact 64-byte compiled appearance input/reference resolution. Missing optional surface channels use renderer defaults, authored color/emission/alpha remain independent, malformed input has no partial output mutation. CPU appearance9+texture4+shading3 pass; full make all builds. Runtime imported GPU override/admission and displayed/performance acceptance remain open; shared GPU/device failure currently blocks GPU launches.
