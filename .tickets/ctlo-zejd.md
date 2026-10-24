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


## Notes

**2026-10-24T01:41:56Z**

RED: empty64x64 appearance bank8454144bytes. Initial8192-word arena gives1048576bytes/bank, but an initial experiment exposed terrain descriptor ranges still derived from geometry capacity (262144bytes exposed versus2113536required), causing an i915 reset. Preserved build/appearance_gpu_reset_kernel.log; checked-in 16-word Vulkan health probe passes all three bounded fences and all words. Added constructor descriptor RED and one shared terrain-storage sizing helper for allocation and bindings28-30. Constructor2/2 and empty-arena growth1/1 now pass; small colored extraction/140-body relocation1/1 passes explicit core+sync validation. Large97028-pixel automatic geometry growth validation running before graphical replay. No further GPU work was launched until health probe passed.

**2026-10-24T01:43:55Z**

After descriptor fix, small colored extraction plus140-body geometry/contact relocation passes with core+sync validation. New97028-pixel component test also passes with core+sync: first allocation-deferral frame preserves source terrain; automatic arena growth then extracts every pixel with exact mass, source consumed only after commit. No new kernel resets after the recorded failed experiment. Initial bank allocation/growth and constructor descriptor coverage pass. Running >64 fracture and graphical memory/support checks next.
