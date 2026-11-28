---
id: ctlo-e56d
status: closed
deps: []
links: []
created: 2026-10-24T07:42:00Z
type: task
priority: 1
---
# Prefer available native Wayland video backend for Vulkan windows

## Acceptance Criteria

When WAYLAND_DISPLAY is available and no SDL video-driver preference is set, initialize SDL with Wayland. Preserve explicit environment or SDL hint overrides. Test backend selection before Vulkan creation, then run the real Wayland UI suite. Track X11-only crash separately if not reproduced natively.


## Notes

**2026-10-24T07:45:52Z**

RED native-session probe selectedx11 (explicit environment/hint cases already passed). GREEN selectswayland without preference and respects both explicit override forms:3/3; wrapper prevents all Vulkan creation. Default-X11 GUI run reportedNo DRI3 then crashed after high CPU/RAM. Same level03 UI test under explicit Wayland/gdb passes1/1 with240 cut-tree ticks and zero stone overlap. Complete native UI/quarter suites underway.

**2026-11-28T06:08:59Z**

The native SDL Wayland UI battery is running on the GL compositor inside hidden persistent Xephyr :93. The backend-selection path is exercised separately. Four failures so far: the original 15-second recreated-window deadline (ctlo-equi), controls_tests waiting for chunk residency on the eighth-step eviction path, the 60-second structural demo subprocess deadline, and residual post-collapse motion in structural_visual_scene_tests (ctowpe-o944). This run uses the unmodified test deadlines.

**2026-11-28T06:16:08Z**

Finished the complete native Wayland UI recipe with SDL_VIDEODRIVER unset on a GL Weston compositor inside Xephyr. All 28 commands ran, with 24 passing. Backend-before-Vulkan checks passed 3/3: native Wayland, explicit environment override, explicit SDL hint. Level03 support and pixel dynamics both passed natively. The X11-only issue is independently documented in ctlo-he9b. The four main recipe failures remain recorded above. The bonfire prerequisite returned 18/21 combustion checks (ct-2cad); its subsequent bonfire display test and CLI were executed independently, with the display timestep exceeding 0.25s and the CLI passing. No display-driver selection failure was observed.
