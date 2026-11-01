---
id: ct-rzrj
status: in_progress
deps: []
links: []
created: 2026-11-01T04:02:18Z
type: bug
priority: 1
assignee: kmd
parent: ctlo-d7vm
tags: [smoke, gas, visual, physics]
---
# Replace smoke display distortion with advected wavelet turbulence synthesis

The current smoke_wavelet is a two-scale smooth hash difference evaluated in the overlay shader. It does not decompose resolved velocity energy, synthesize a band-limited divergence-free velocity field from that energy, or advect a finer density field. The fixed phase can read as static dither despite advected coordinates. Implement a true 2D adaptation of Kim et al. 2008: local resolved velocity wavelet energy, band-limited curl noise, advected and strain-regenerated coordinates, fine smoke density advection, boundary deformation with restrained interior modulation. Conserve species and energy in the physical grid and keep Beer-Lambert opacity density-dependent.

## Acceptance Criteria

RED GPU tests demonstrate lack of velocity-dependent band energy, stationary pattern at zero flow, plume boundary not transported by synthesized velocity, coordinate strain failure, and conservation. GREEN tests pass at quarter-native display resolution with visible nonstationary edge detail, bounded interior contrast, finite physics, paused replay identity, and frame budget. Run full make test and bonfire/smoke demos.


## Notes

**2026-11-01T04:53:16Z**

Intermediate GPU wavelet coordinate/potential passes pass quarter-native motion and paused replay checks. Persistent fine-density advection remains; keep ticket open.

**2026-11-01T05:02:14Z**

1000-frame quarter-native bonfire remains just over strict 30ms worst-frame gate: 30.565-30.905ms in two untraced runs, despite ~18ms mean. Frame API trace shows occasional present waits of 12-18ms alongside ~15-17ms GPU fence. Potential-pass dead fetch removal passed GPU test but did not resolve the worst-frame gate.
