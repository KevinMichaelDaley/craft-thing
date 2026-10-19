---
id: ctr-ebl5
status: in_progress
deps: []
links: []
created: 2026-10-19T01:33:39Z
type: task
priority: 1
parent: ctr-6fii
tags: [renderer, vulkan, lighting, performance]
---
# Remove water-induced light shafts and optimize propagated lighting

User reports that water creates light shafts. Fix that artifact while optimizing the actual directional radiance transport noninteractively in a separate worktree. Preserve partial leaf transmission, diagonal skylight, lateral ambient fill and underground attenuation.

## Acceptance Criteria

RED-GREEN-REFACTOR regressions demonstrate that water neither emits light nor introduces spurious light shafts. Preserve canopy shadow recovery, opaque/unknown blocking, colored moments and stable paused frames. Benchmark Vulkan on this machine at 480x270 and 640x360 with unchanged physics and report isolated GPU timings versus the 32.186ms renderer baseline. Acceptance is total simulation plus rendering frame time <30ms on this machine, at both 480x270 and 640x360 with unchanged physics fidelity; renderer stage times are diagnostic only. No interactive demo or UI automation; commit repository tools and tests, update tickets, run full headless suite and push.


## Notes

**2026-10-19T01:34:04Z**

USER CORRECTION: acceptance is TOTAL FRAME TIME <30ms on this machine, including simulation and rendering, not a2ms renderer-only budget. This supersedes the2ms target in the original acceptance. Benchmark the complete frame at current scene configuration and640x360; preserve simulation fidelity and light transport correctness. Work noninteractively.

**2026-10-19T01:51:23Z**

Water regression verified RED for false 0.08 bulk-water extinction and for wet solids losing opacity. GREEN clear-water optics: zero solid occlusion and no emission; wet stone remains opaque; terrain/moving leaves retain partial opacity. Compact FP32-energy, signed10-bit direction-ratio transport reduces per-cell intermediate storage256 to120 bytes plus unchanged64-byte final field. Specialized phases, source/reach prepass,32-lane subgroup scans with validated shared fallback. Lighting11/11 and scene1/1 pass core+sync. Full-frame benchmark now measures actual tick + render + submissions/waits at480x270 and640x360,16 pressure sweeps unchanged. Current contended means46.969ms/76.511ms (renderer27.582/50.682; light20.917/39.803), so <30ms acceptance remains unmet and ticket stays in progress. Existing demo PID288414 untouched; i915 /proc fdinfo measured93.3% render-engine occupancy over7.61s. No isolated acceptance claim. Full headless suite pending.
