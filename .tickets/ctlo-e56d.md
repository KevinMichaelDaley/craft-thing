---
id: ctlo-e56d
status: in_progress
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
