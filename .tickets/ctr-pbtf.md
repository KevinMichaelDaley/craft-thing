---
id: ctr-pbtf
status: closed
deps: [ctr-9k5m]
links: []
created: 2026-10-14T03:12:55Z
type: task
priority: 1
parent: ctr-6fii
tags: [renderer, vulkan, appearance, lighting]
---
# Implement point and sun transport into a directional RGB lighting field

Implement source-driven GPU-parallel sky and emissive radiance transport into RGB energy/direction moments. Use directional beam buffers and tiled affine scans with angular redistribution; document attenuation, height and units. Leaves partially transmit light and underground illumination fades with travel distance.

## Acceptance Criteria

Point and sun propagation, opposing lights retaining energy, differently colored directional lights, emitter in a hole, zero-distance finite values, unknown boundaries, partial terrain/body leaf transmission, doorway transport, underground attenuation and scan-tile boundaries are tested. No receiver raycasts or serial all-source/all-receiver pass. Preserve HDR composition and residency invalidation.


## Notes

**2026-10-19T00:49:55Z**

User requires actual sky/emitter radiance propagation, partial leaf transmission and progressive underground attenuation; raycast visibility plus normal shading is insufficient. Replace receiver-ray transport with persistent directional beams, emissive injection and GPU-parallel tiled affine scans with angular redistribution. Remove unconditional ambient illumination underground.

**2026-10-19T01:14:44Z**

Wide canopy regression reproduced insufficient sideways ambient fill (far shadow energy <65% of open sky). Separate direct radiance from two diffuse scattering orders, accumulate their directional moments, and use independent diffuse transport coefficients so lateral ambient spread does not extinguish direct daylight. Additional graphical review and performance measurement in progress.

**2026-10-19T01:20:32Z**

Diagonal sky transport plus lateral diffuse propagation passes focused Vulkan core/synchronization validation: 10 lighting cases and integrated leaf-body optics. Wide foliage canopy regression compares against unobstructed radiance at identical coordinates: near shade 56%, far shade 71%, exceeding the required 10 percentage-point recovery. Sealed opaque layer and unknown-residency blocking remain verified.

**2026-10-19T01:25:23Z**

Final complete make -j4 -i test run finished: only the two pre-existing authored_stage_tests failures at lines67/135 remain (ctr-ojvy); no new regression failures. Focused core+sync tests10/10 and integrated foliage optics1/1 pass; real level03 UI1/1 passes with stable paused output and preserved support behavior. Final screenshot build/ambient_scene_final.png. GPU performance remains explicitly open on ctr-21eh (32.186ms total at480x270).
