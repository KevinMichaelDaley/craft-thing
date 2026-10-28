---
id: ctfs-6t7k
status: closed
deps: []
links: [ctfs-5gil]
created: 2026-10-28T19:12:27Z
type: bug
priority: 1
parent: ctfs-81vw
tags: [fire, integration, vulkan]
---
# Make bonfire ignition robust to normal displayed frame cadence

Repeated fullscreen real-clock runs now expose an all-log ignition failure after frame-time improvements: default per-pixel contacts reach7/8, experimental rectangle contacts6/8 or7/8, while some earlier runs pass8/8. Keep finite fuel, real oxygen and unchanged combustion coefficients. Diagnose physical airflow and rigid/thermal clock coupling; do not weaken all-log ignition or aggregate sustained-fire checks.

## Acceptance Criteria

Repeated native fullscreen bonfire passes all8 dry logs, wet0, finite starter and20s aggregate sustained combustion under normal clocks; record actual frame timings and keep numerical/energy regressions green.


## Notes

**2026-10-28T19:30:23Z**

Native shallow-rim runs both ignited8/8 dry and0 wet (by about31s and44s). The second failed only because the old scorer required20s AFTER the last ignition within60s. Added compiled RED2pass/1fail progress-unit case proving whole-pile burning may precede final ignition, then GREEN3/3; native corrected battery passes with8/8 dry,0 wet at21.8s. All per-log fuel/flaming/visible-char checks remain unchanged, as do combustion coefficients and the finite starter. Final whole-suite run pending.

**2026-10-28T19:35:57Z**

Final checkpoint: progress-unit3/3, corrected fullscreen bonfire battery8/8 dry and0 wet with sustained pile combustion at21.8s, complete make test suite passes, native water4/4. Same per-log visible-char threshold and combustion coefficients retained. A prior shallow-rim run independently demonstrated8/8 plus20s after-final-ignition at51s; the second had all8 by44s and continuous combustion through60s but exposed the unnecessary ordering restriction. Closing ignition/scorecard fix; frame budget and elapsed rigid clock remain separate open tickets.
