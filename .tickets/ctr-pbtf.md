---
id: ctr-pbtf
status: in_progress
deps: [ctr-9k5m]
links: []
created: 2026-10-14T03:12:55Z
type: task
priority: 1
parent: ctr-6fii
tags: [renderer, vulkan, appearance, lighting]
---
# Implement point and sun transport into a directional RGB lighting field

Implement GPU-parallel light culling and visibility before aggregation, conservative bounded hierarchy traversal or measured parallel sun scans, RGB energy/direction moments with documented attenuation/height/units. Start direct hard shadows then bounded soft-source sampling.

## Acceptance Criteria

Point and sun shadows, opposing lights retaining energy, differently colored directional lights, emitter in a hole, zero-distance finite values, conservative traversal overflow/unknown boundaries, HDR composition and streaming sources are tested. No serial all-source/all-receiver pass.


## Notes

**2026-10-19T00:49:55Z**

User requires actual sky/emitter radiance propagation, partial leaf transmission and progressive underground attenuation; raycast visibility plus normal shading is insufficient. Replace receiver-ray transport with persistent directional beams, emissive injection and GPU-parallel tiled affine scans with angular redistribution. Remove unconditional ambient illumination underground.
