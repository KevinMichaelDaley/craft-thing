---
id: ctfs-3u4w
status: closed
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

**2026-10-31T21:30:45Z**

Native X11 verification on this GPU: 20-step rising-plume test control lateral momentum 1.16479984, local vorticity 1.23008959 (+5.6%) with mass/O2 conserved; low-density soot display red contrast 21 levels against dark background. Fullscreen 1920x1080 window, quarter-native 480x270 physics/render with opacity 2000: direct window recording build/ctfs_strength2_opacity2000_plume.mp4 shows broad moving brown plume; bonfire run passed 8/8 dry and 0/8 wet by 21.7 s. Local stencil and existing pass only; full suite and benchmark pending.

**2026-10-31T21:48:19Z**

Final validation: tools/with-vulkan make -j8 test passed exit 0 after all source changes. Strict 1000-frame quarter-native benchmark repeated: first run means 16.439/16.609 ms, worst 23.266/30.310 ms (one 0.310 ms gate miss); clean repeat means 16.549/16.684 ms, worst 20.186/21.494 ms, exit 0. No added global scan or iteration. Solid-char hotspot remains tracked separately as ct-li7d.
