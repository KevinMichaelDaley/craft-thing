---
id: ct-d50u
status: closed
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

**2026-10-08T06:54:15Z**

During dun-9qub validation, Wayland window runs stalled after attaching/committing the first Vulkan image and requesting a wl_surface frame callback. org.gnome.ScreenSaver.GetActive returned true on every check: the desktop is locked. X11 cannot present on this machine because DRI3 is unavailable. Owned test processes were bounded and killed by timeout; no compositor or lock settings were changed. An unlocked desktop is required to retry the new quarter-native stack scene, existing window regressions, and UI smoke checks. This evidence explains these runs, but does not establish that every earlier intermittent stall had the same cause.

**2026-10-09T02:42:20Z**

With ScreenSaver.GetActive=false, the previously stalled dun-9qub window scene completed. The full quarter-native suite (six window scenes including successive recreation) and make test_ui (five controls tests plus ten smoke scenes) now pass under Intel Vulkan validation. No compositor settings or presentation code changes were needed for these runs. Leave this ticket open for its broader intermittent-stall diagnostics and repeated bounded-run acceptance.

**2026-10-11T02:33:42Z**

During ct-cnsq verification, Wayland visual scene repeatedly stalled in poll_schedule_timeout with ScreenSaver.GetActive=false. Some runs stalled after producing partial replay frames, so this is not solely initial window creation or a locked desktop. Owned test processes were bounded and retried; an earlier unchanged scene completed all 120 exact settled-frame comparisons. No compositor settings or presentation assertions changed.

**2026-10-11T02:35:37Z**

Final trace again stops after wl_surface.frame + attach/damage/commit, with no subsequent frame callback; org.gnome.Mutter.DisplayConfig.PowerSaveMode is 0 and ScreenSaver.GetActive is false. Disabling SDL libdecor preference did not resolve it. Normal GPU headless tests complete; this remains a separate presentation diagnostic issue.

**2026-10-11T02:56:16Z**

RED regression confirmed MAILBOX is available on this Intel Wayland surface but renderer forced FIFO. GREEN now selects supported MAILBOX (FIFO fallback), pumps SDL events, limits acquisition to one second, allocates a presentation semaphore per swapchain image, and fences rendering completion independently of display. Initial three hide/restore/recreate cycles pass without presentation queue-idle waits. Previously stalled paced concave-body scene now completes, including all 120 exact settled frames. Full post-refactor suites running.

**2026-10-11T03:01:31Z**

Final verification on Intel Iris Xe Wayland with Vulkan validation: make test passes all 143 headless cases; full make test_quarter_native and make test_ui pass, including all six rigid-window cases, two paced concave visual runs with 120 identical settled frames each, six new hidden/recreated-window cycles with exact kinematic movement, controls 5/5, and all ten UI smoke scenes. No stalled retries or Vulkan validation errors in these batteries. Three 90-frame regression cycles measured acquire totals 6.682–7.662ms and presentation enqueue totals 9.733–10.168ms separately from render-fence waits 731.030–808.544ms. Physics stage graph is unchanged. MAILBOX is selected only when supported; FIFO remains the portable fallback. Refreshed the stable rigid_concave.gif replay.
