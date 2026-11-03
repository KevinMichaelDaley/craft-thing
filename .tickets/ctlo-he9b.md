---
id: ctlo-he9b
status: closed
deps: []
links: []
created: 2026-10-24T07:45:52Z
type: bug
priority: 2
---
# Diagnose X11-selected GUI startup crash on this machine

Automatic SDL backend was x11 while WAYLAND_DISPLAY was available. level03_support_tests and pixel_dynamics_visual_tests reported Mesa No DRI3, consumed about2GB CPU compilation memory, then segfaulted. The level03 test passes normally under explicit Wayland in gdb. Native backend preference is addressed by ctlo-e56d; X11-specific stack/device selection still needs isolated investigation. Logs build/inline_current_ui_suite.log and build/ui_wayland_debug.log.


## Notes

**2026-11-03T04:43:51Z**

Root cause and closure: the X11 startup crash path (Mesa stack, No DRI3, ~2GB shader-compiler memory, segfault) could win device selection because dc_gpu_pick_device took the FIRST adapter meeting the capability check, with no vendor/type preference and no software-renderer guard, unlike tools/diagnostics/vulkan_probe.c which already skips VK_PHYSICAL_DEVICE_TYPE_CPU. GREEN be8eae50/d2380376: dc_gpu_device_score rejects CPU-type devices, prefers discrete over integrated over other hardware, ties keep enumeration order; pick_device scores all candidates and the app now prints 'Vulkan device: <name> (type N vendor XXXX)' at every startup for crash diagnostics. On this machine X11 now deterministically selects 'NVIDIA RTX A2000 12GB'. device_selection_tests 5/0; level03_support_tests and pixel_dynamics_visual_tests pass under DISPLAY=:1 X11; video_driver_tests 2/0; thermal display under validation 2/0. Repeat runs showed no recurrence. Remaining machine-specific crash trigger (driver update regression) would now be immediately visible in the startup device line.
