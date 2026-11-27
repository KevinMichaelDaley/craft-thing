---
id: ctosd-silg
status: closed
deps: []
links: []
created: 2026-11-27T01:32:32Z
type: task
priority: 1
assignee: kmd
parent: ctswc-xr49
tags: [offscreen, vulkan]
---
# Share the Vulkan device across isolated simulation contexts

Restore shared device lifetime support for offscreen physics on the current Vulkan runtime.

## Design

Device, instance, validation and pipeline cache live until the last context releases them. Each context owns its simulation buffers, commands and scene state. Exercise both destruction orders and the partial-construction path.

## Acceptance Criteria

A Vulkan child context on its parent's instance, device and queue steps independently and can be released before or after the parent. Reject invalid arguments and a failed child build without affecting the existing context. Pass core and synchronization validation.

## Notes

**2026-11-27T01:55:00Z**

RGR 8638a588, 728a91a4, and 7e8a2dad. All three lifecycle checks pass under core and synchronization validation on the software Vulkan ICD, including an independent physics tick, both destruction orders, and cleanup after a failed build. The software test adapter is external to the source tree and only selects the software ICD for validation. The app and host checks passed, Clang static analysis was clean, and all 143 compiled prerequisites of make test built after correcting the older reactive fixture. The RTX GPU and window-parent checks plus the full suite remain queued; keep this ticket in progress until native results are recorded.

**2026-11-27T02:17:32Z**

The exact native GPU suite completed on the RTX A2000 with core and synchronization validation. All 14 cases passed, including independent shared-device physics, both destruction orders, and failed child construction. The standalone X11 window-parent probe and whole make test run remain queued.
