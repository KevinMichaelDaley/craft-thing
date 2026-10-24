---
id: ctlo-zejd
status: in_progress
deps: []
links: [ctlo-sgja, ct-tu55]
created: 2026-10-24T01:30:07Z
type: bug
priority: 1
tags: [vulkan, physics, memory]
---
# Bound initial body appearance storage while retaining dynamic geometry growth

An empty 64x64 runtime reserves 8454144 bytes per body color bank because its initial geometry arena is proportional to the resident terrain pool. The graphical quarter-native material fixture reports99.670MiB mapped against a pre-appearance72MiB budget. Allocate a modest initial arena and grow on demand; terrain component scratch must remain sized for every resident cell independently. Update the visual memory accounting to separate required authored-color/dynamic metadata from the original physics budget, without weakening the physics or frame-time gates.

## Acceptance Criteria

An empty runtime reserves no more than1MiB per appearance bank and can grow beyond that budget without a fixed geometry/body limit. Automatic extraction, fracture, exact geometry/material colors and counted checkpoints survive relocation. Terrain labels/candidates remain large enough for all resident cells. Actual displayed saved-scene frame budget and graphical memory checks pass.

