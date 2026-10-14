---
id: ctr-21eh
status: open
deps: [ctr-7tl5, ctr-ddtf]
links: []
created: 2026-10-14T03:12:56Z
type: task
priority: 1
parent: ctr-6fii
tags: [renderer, vulkan, appearance, lighting]
---
# Validate textured concave scenes and renderer scaling on this machine

Build actual displayed visual battery with two different concave textured bodies, varied solids, sun and colored point lights, motion/fracture/wet material, narrow shadows and paused close-ups. Add stage GPU timers and dirty/unchanged, viewport, 1/4/16-light complexity sweeps.

## Acceptance Criteria

Visual captures and assertions demonstrate coherent grain/relief, exact holes/shadows, stable paused frames and persistent textures. Vulkan/SPIR-V/headless/UI/quarter batteries pass. Reference480x270 sun+4 lights targets <=2ms added mean renderer GPU time; report cold/tail/VRAM and actual displayed FPS without reducing physics or weakening gates.


## Notes

**2026-10-14T04:55:44Z**

Device diagnostic under an explicitly granted30sec lease: unchanged previously passing96-texel texture check instrumented with linker-only progress wrappers. Context creation and texture resource/pipeline creation succeeded; first fr_texture_gpu_generate submission never completed before timeout25sec exit124. No validation error printed. Lease released, no renderer process remains. Display/performance fixtures compile but remain unrun; acceptance and FPS claims are blocked on shared submission recovery.
