---
id: ct-fan4
status: open
deps: []
links: []
created: 2026-10-11T21:14:45Z
type: bug
priority: 2
tags: [vulkan, visual]
---
# Investigate intermittent quarter-native concave visual SIGSEGV

During ct-il9b full quarter-native validation, existing rigid_dynamics_visual_tests exited with SIGSEGV once before buffered test output. No core/backtrace was available. The full quarter-native retry, standalone stdbuf run, GDB run and freshly built ASAN/UBSAN executable all passed, including 120 identical settled frames. No crash fix has been claimed. Original log /tmp/ct-il9b-quarter-full.log, retry /tmp/ct-il9b-quarter-retry.log and debug logs /tmp/ct-il9b-visual-{backtrace,plain,asan}.log are local diagnostic evidence.

## Acceptance Criteria

Capture a reproducible failure and actionable stack trace or driver diagnostic; repair the demonstrated cause and run repeated quarter-native visual and presentation batteries without recurrence.

