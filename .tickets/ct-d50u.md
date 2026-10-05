---
id: ct-d50u
status: open
deps: []
links: []
created: 2026-10-05T02:14:49Z
type: bug
priority: 2
parent: dun-v2vu
tags: [vulkan, wayland, ui]
---
# Investigate intermittent Wayland presentation stalls in Vulkan smoke tests

Quarter-native --smoke-native intermittently waited over 100 seconds with the main thread polling while worker threads were idle. A fresh retry on the same Intel Iris Xe Vulkan driver completed successfully. Similar window startup delays occurred in the two-box regression. Preserve GPU physics results; investigate compositor/event dispatch and swapchain acquisition/presentation rather than treating the stall as solver cost.

## Acceptance Criteria

Repeated quarter-native and UI smoke runs complete within bounded time, including successive window recreation; diagnostics identify presentation wait time separately from physics; no regression in Vulkan validation or the quarter-native physics budget.


## Notes

**2026-10-05T04:21:26Z**

During ct-l35t validation, quarter-native --smoke-native stalled for over 90 seconds in poll_schedule_timeout, and default --smoke-display stalled for over 70 seconds. Only the owned test processes were killed. Both unchanged smoke checks passed on standalone retry; all remaining UI scenes passed. Convex window scene including repeated create/destroy, camera eviction and save/reopen passed twice. No presentation/compositor workaround or assertion relaxation introduced.
