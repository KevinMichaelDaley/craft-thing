---
id: ctfs-5gil
status: in_progress
deps: []
links: []
created: 2026-10-28T04:18:38Z
type: bug
priority: 0
parent: ctfs-81vw
tags: [fire, performance, vulkan]
---
# Restore displayed bonfire frame budget below30ms before siege expansion

Actual fullscreen Wayland Vulkan benchmark at480x270 physics/render with640x448 halo regressed to101.693ms mean106.760ms worst. First run reached1154 measured frames before120s bound. Functional native tests and full regression are green, but performance is not. Profile and fix inline before coal/siege expansion; do not revisit stable smoke numerics or fake performance through lower physical clock/resolution, flame point caching or headless-only evidence.

## Acceptance Criteria

Real displayed full-frame mean and worst below30ms on this machine over the autonomous finite-fuel bonfire, including all logs catching and sustained moving flames. Preserve current physics/render resolution, normal physical clock and eight rigid logs per pit, wet controls, finite energy/O2 and solid-only subtle2D conduction. Profiling/tools stay in repository; no fixed body counts. Full relevant native integration tests and regression pass, changes pushed.


## Notes

**2026-10-28T18:32:19Z**

Inline continuation: thermal phase specialization improved a short real-clock fullscreen coarse trace from roughly111ms to95.552ms mean/98.797ms worst. Grouped solver reductions95.865/98.084ms and contact boundary cache97.286/100.538ms did not improve this; neither enabled. Combined thermal sample/metadata lookup now passes native water4/4 and combustion feedback6/6. Full performance acceptance remains RED; short profiles are diagnostics, not completion. Investigating exact collision and thermal work reduction; no resolution/clock changes.
