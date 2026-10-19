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

Visual captures and assertions demonstrate coherent grain/relief, exact holes/shadows, stable paused frames and persistent textures. Vulkan/SPIR-V/headless/UI/quarter batteries pass. At 480x270 and 640x360 with sun+4 lights, total simulation plus rendering frame time must be <30ms on this machine; report cold/tail/VRAM and actual displayed FPS without reducing physics or weakening gates.


## Notes

**2026-10-14T04:55:44Z**

Device diagnostic under an explicitly granted30sec lease: unchanged previously passing96-texel texture check instrumented with linker-only progress wrappers. Context creation and texture resource/pipeline creation succeeded; first fr_texture_gpu_generate submission never completed before timeout25sec exit124. No validation error printed. Lease released, no renderer process remains. Display/performance fixtures compile but remain unrun; acceptance and FPS claims are blocked on shared submission recovery.

**2026-10-17T08:40:44Z**

Merged renderer fb395f4 with optimizer checkpoint 7b65fd5. Fresh shaders and full headless tests pass with core plus synchronization validation; displayed renderer visual fixture and full test_ui pass. Isolated no-validation 480x270 sun+4 point-light acceptance FAILS unchanged <=2ms added GPU gate: mean total10.563ms (texture0.123 optical2.350 hierarchy0.145 light7.030 compose0.752). Earlier timing20.266ms overlapped UI and is not authoritative. Performance remains open; no FPS or final renderer completion claim. Logs build/renderer-merge-performance-isolated.log and renderer-merge-visual.log.

**2026-10-19T01:21:42Z**

Latest actual directional transport with cardinal direct light, diagonal sky transport and lateral diffuse fill: isolated no-validation 480x270 sun+4 points still FAILS unchanged <=2ms gate. Total32.186ms: texture0.232 optical2.573 hierarchy0.165 light28.302 compose0.739. Lighting correctness and graphical level03 checks pass with core+sync validation, but GPU cost is unacceptable and this performance ticket remains open. Persistent 256-byte/cell intermediate beams plus64-byte field and scan traffic/barriers are optimization candidates; preserve transport/foliage/shadow recovery assertions. Logs build/ambient_performance.log, ambient_focused.log, ambient_scene_final.log.

**2026-10-19T01:34:04Z**

User supersedes the old <=2ms renderer-only target: total frame time must be <30ms on this machine, including simulation and rendering. Use full-frame timings as acceptance; retain per-stage GPU measurements for diagnosis.
