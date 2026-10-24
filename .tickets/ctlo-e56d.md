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

