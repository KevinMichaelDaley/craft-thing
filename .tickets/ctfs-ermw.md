---
id: ctfs-ermw
status: closed
deps: []
links: []
created: 2026-10-28T03:33:23Z
type: feature
priority: 1
parent: ctlo-6syy
tags: [siege, fire, materials]
---
# Admit finite coal and char projectile materials as actual rigid geometry

Extend the shared terrain/body material mapping, density, extraction and profile generation for coal and char. The siege projectile must use its own material density/fuel/thermal stock, not wood painted black.

## Acceptance Criteria

RED-GREEN-REFACTOR. Native GPU coal/char bodies preserve actual material through admission, mass/inertia, collision, rendering, burning, and snapshot restore. A cold coal control stays cold; the finite hot shot emits state-driven heat/flame and leaves finite char/ash. Shared C/GLSL mapping round trips correctly without extending physics resolution or introducing fixed body counts.


## Notes

**2026-10-28T21:17:35Z**

Resumed on feat/fire-smoke-reactive-materials. RED/GREEN commits eb26da2/1281c3b map coal and char to distinct rigid body materials and density, with direct GPU mass/checkpoint regression. RED/GREEN commits 42a5528/7e30277 admit extracted coal/char as rigid-capable pixels retaining finite reactive parcels; 7/7 reactive-cell GPU tests pass. Full make test running. Remaining acceptance: hot/cold combustion and char/ash product regression, rendered projectile, and assembled launcher scene integration. Parent ctlo-6syy remains in progress; no release fixture yet.

**2026-10-28T23:34:57Z**

Coal/char mapping, density, rigid extraction, finite reactive state, cold control, hot combustion, char/ash and checkpoint regressions pass. The catapult display fixture renders actual rigid coal and launches it into the wall. Full make test is running.
