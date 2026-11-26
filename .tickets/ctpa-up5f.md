---
id: ctpa-up5f
status: open
deps: []
links: [ct-d3oz]
created: 2026-11-26T11:10:52Z
type: bug
priority: 2
assignee: kmd
parent: ctfs-81vw
tags: [fire, tests, performance]
---
# Diagnose the displayed bonfire 250 ms frame spike under GPU contention

The displayed bonfire admitted the finite 1800 K starter on both the pinned main build and the sandbox atmosphere branch, then stopped at tests/app/bonfire_tests.c:178 because the next frame interval exceeded 250 ms. The strict suite was running the forest renderer on the same RTX A2000. Both recordings are in /tmp/craft-play-air-bonfire.log and /tmp/craft-play-air-bonfire-baseline.log.

## Acceptance Criteria

Reproduce the event with frame and GPU wait timings, measure the display in isolation and with a competing Vulkan workload, and trace which work exceeds the frame budget. Keep the existing timed native acceptance, all-log ignition and sustained-fire checks.


## Notes

**2026-11-26T11:12:21Z**

Both runs used the same RTX A2000 and failed immediately after the finite starter admission. The baseline is b800354f. The fire-ray integration and the sandbox save/reload passed their independent functional checks.
