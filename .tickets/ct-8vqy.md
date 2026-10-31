---
id: ct-8vqy
status: open
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

