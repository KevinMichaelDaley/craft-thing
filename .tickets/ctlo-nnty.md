---
id: ctlo-nnty
status: open
deps: []
links: []
created: 2026-10-19T21:32:09Z
type: task
priority: 1
parent: ctr-ebl5
tags: [vulkan, performance, fluid]
---
# Skip scheduled fluid work only while resident state is provably dry

Actual displayed loaded level03 at native640x360 remains67ms mean with renderer4-5ms; scheduled fluid work on the dry world costs22-30ms per captured tick. Track conservative fluid-work possibility from cold uploads, velocities, water brush/source and prop/authoring publication. Never skip after any possible water activity, and leave explicit standalone fluid diagnostics intact.

## Acceptance Criteria

Dry scheduled ticks issue no fluid phases without changing rigid/granular simulation. Uploading water or velocity, painting water, starting a source or publishing water assets wakes scheduled fluid immediately; wet state remains conservative and mass/flow oracles pass. No fidelity, cadence, pressure or timing-gate reductions. Actual displayed application and strict complete-frame benchmarks improve; full suite checked.

