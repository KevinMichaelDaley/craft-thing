---
id: ctr-11i9
status: in_progress
deps: []
links: []
created: 2026-10-17T08:59:40Z
type: chore
priority: 1
tags: [renderer, physics, assets]
---
# Integrate prop and physics checkpoints with renderer and export level_piece01

Merge exporter7469573 then physicsa068273 into renderer; validate shared ABI and imported pending REDs. Export actual128x128 level_piece01 source using Layer1 color, Layer2 red wood/black stone/yellow dirt/blue water/transparent preserve existing, Layer3 opaque white terrain. Preserve original sources and add reusable profile with deterministic validated package.


## Notes

**2026-10-17T09:27:19Z**

User expanded acceptance to actual material placement gameplay, then corrected procedural stone burial. Added parallel terrain-only preflight/publication (no component CPU solve), exact water/MPM material initialization, authored resident color channel and UNKNOWN/body/role rollback. Actual128x128 asset centered on chunk0,0 across64-cell chunks. Authored hashed ground.bin defines surrounding cold-generated soil/air boundary so source ground is exposed; immutable streamer profile extends edges, preserves saved chunks. Corrected dirt renderer defaults in matched CPU/GLSL. SourceASE byte-identical. Selected sync validation: textureGPU1/1, terrainprop2/2, actualwindow scene central chunk/4pausedframes/brush/12physics ticks PASS, no VUID/SYNC diagnostics. Full headless battery ran through all groups with only2pre-existing ctpf-iueo REDs; final UI pending. Persistent tools/demo_level_piece01 launches scene paused.

**2026-10-17T09:29:27Z**

Final UI suite exited0. Final selected actual-window scene run plus textureGPU/terrainprop checks passed core+sync validation with no VUID/SYNC hazards. Ground-boundary regressions2/2 and offline profile/ground companion tests4/4 pass; entire headless recipe completed with exactly the2pre-existing authored staging failures and no others (make -i used explicitly to continue, not a passing-suite claim). Final native launcher rebuild guards now include renderer/prop/authored shaders and shared asset headers; interactive launch and push remain.
