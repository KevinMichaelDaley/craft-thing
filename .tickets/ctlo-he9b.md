---
id: ctlo-he9b
status: open
deps: []
links: []
created: 2026-10-24T07:45:52Z
type: bug
priority: 2
---
# Diagnose X11-selected GUI startup crash on this machine

Automatic SDL backend was x11 while WAYLAND_DISPLAY was available. level03_support_tests and pixel_dynamics_visual_tests reported Mesa No DRI3, consumed about2GB CPU compilation memory, then segfaulted. The level03 test passes normally under explicit Wayland in gdb. Native backend preference is addressed by ctlo-e56d; X11-specific stack/device selection still needs isolated investigation. Logs build/inline_current_ui_suite.log and build/ui_wayland_debug.log.

