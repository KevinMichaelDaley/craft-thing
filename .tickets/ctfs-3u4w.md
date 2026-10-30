---
id: ctfs-3u4w
status: in_progress
deps: []
links: []
created: 2026-10-27T20:07:05Z
type: bug
priority: 1
parent: ctfs-81vw
tags: [fire, smoke, gameplay]
---
# Restore sideways rolling in transported bonfire plumes

User sees straight vertical flame streaks. Restore local fluid curl/entrainment, without a screen-space flame animation or external fake wind. Keep bounded GPU work and finite gas budgets.

## Acceptance Criteria

Displayed rising-plume regression increases sideways motion from local vorticity while preserving species; fullscreen bonfire visibly rolls sideways; no additional global scans or solver iterations.


## Notes

**2026-10-27T20:08:50Z**

Native Wayland RED: rising plume had identical lateral momentum with feature toggle (1.27975959). GREEN: local immutable-cache curl confinement produces 1.30133897 while retaining mass/O2. It converts existing total energy between kinetic/internal forms, uses fixed local stencil, and has bounded velocity change; no random wind, extra iterations, global scans or animation mask. Enabled in tools/smoke_profile; combined fullscreen bonfire validation ongoing.

**2026-10-28T01:59:51Z**

Fixed zero-axial-velocity early-out in all optimized transport variants and quiet-tile density/enthalpy wakeup. The second split sweep examines first-sweep results. Corrected regression uses the large-CFL optimized path: original transport diagonal299.999990K, updated310.212775K, energy drift0. Vorticity reduced to strength1 and 0.1-local-speed per-step bound after user observed excessive lateral spread. Whole-gas underflow cleanup bounded below1e-30 mass and1e-24 energy/momentum; resolved tracers retained.

**2026-10-30T03:54:11Z**

2026-10-29 validation attempt: tools/test_bonfire initially needed make tools; pinned Vulkan toolchain installed. Native battery then failed at window creation because this session has DISPLAY=:1 but no Wayland socket. Direct X11 gas_display_tests vorticity also cannot reach the GPU step: Large-CFL transport requires compute subgroup shuffle and float32 denormal preservation, unavailable on this host. No implementation change or displayed acceptance claim; keep ticket in_progress for a capable Wayland/Intel session.
