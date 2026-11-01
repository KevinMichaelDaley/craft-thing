---
id: ct-8vqy
status: closed
deps: []
links: [ctfs-3u4w]
created: 2026-10-31T22:58:42Z
type: feature
priority: 1
assignee: kmd
parent: ctfs-81vw
tags: [smoke, gas, visual, physics]
---
# Add energy-conserving subgrid motion to transported smoke plumes

The fullscreen bonfire smoke remains visually too smooth. Add small-scale plume variation through the gas velocity/transport path using a deterministic, local, divergence-free perturbation driven by resolved buoyancy and shear. Any added kinetic energy must be debited from existing internal or resolved kinetic energy, with finite density and species budgets; do not use a screen-space animation, random wind, or an unaccounted heat source. Preserve density-dependent Beer-Lambert smoke opacity and optical thickness behavior.

## Acceptance Criteria

RED GPU regression reproduces overly smooth transported smoke. GREEN validates more spatial and temporal plume detail at game resolution while preserving mass, each gas species, total energy, and unbiased mean momentum within numerical tolerance; deterministic seed/replay; no plasma-temperature transient; fullscreen bonfire remains 8/8 dry and 0/8 wet; no extra global scan or unbounded solver iteration; benchmark and full make test pass.


## Notes

**2026-11-01T00:23:09Z**

Implemented shear-driven, divergence-free subgrid gas momentum with no added energy; added advected material coordinates for band-limited smoke detail and curl displacement of the plume boundary. Quarter-native GPU regressions verify deterministic replay, gas stock and energy budget, visual motion versus stationary error, displaced edge, unchanged paused frames and unchanged gas state after rendering. Full make test passed (846 RUN cases). Final fullscreen bonfire passed 8/8 dry and 0/8 wet by 30.3s; max gas 1716.5K. Repeated 1000-frame x11 benchmark passed means 17.734/17.843ms, worst 28.226/28.727ms. Visual method follows the material-coordinate advection and local energy idea in Kim et al. 2008 Wavelet Turbulence; it is a two-band approximation, not the full paper algorithm.

**2026-11-01T00:31:05Z**

Follow-up visual tuning: keep advected high-frequency structure, but confine strong displacement to the smoke boundary and make interior optical modulation soft at quarter-native resolution.

**2026-11-01T00:54:18Z**

Visual tuning follow-up: reduced interior wavelet optical gain to 0.6 and blend curl-displaced density by relative optical-depth change, so strong deformation is concentrated at smoke edges. Quarter-native GPU regression: reference interior red range 35..40, dense interior 43..45, displaced boundary 14 cells, advected image error 42 versus stationary 53; paused replay and gas-state identity pass. Fullscreen bonfire passed 8/8 dry and 0/8 wet by 33.3s, final max gas 1416.9K. Displayed 1000-frame benchmark means 17.459/17.667ms and worst 28.113/27.808ms. Full make test passed 846 RUN cases with zero failures. Capture: build/ct-8vqy-smoke-soft.mp4.
