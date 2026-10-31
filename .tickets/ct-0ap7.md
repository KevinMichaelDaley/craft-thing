---
id: ct-0ap7
status: in_progress
deps: []
links: []
created: 2026-10-31T04:55:31Z
type: feature
priority: 2
assignee: kmd
tags: [gas, thermal, vulkan]
---
# Apply Fourier gas conduction when combustion module is disabled

Current gas-only solver transports total energy by Euler flux and uses practical Rusanov mixing, but the explicit Fourier gas-gas conduction pass is recorded by the thermal/combustion module. General gas-only worlds therefore lack species-capacity-weighted molecular heat diffusion through stationary nitrogen/air.

## Acceptance Criteria

RED gas-only GPU test shows stationary temperature gradient exchanges heat without corresponding species mass exchange, including closed gates and partial cells. GREEN finite-volume Fourier term runs in all gas policies, conserves total energy and mass, uses the shared air conductivity and cell geometry, remains stable for thin cells, passes make test and displayed performance gate.

