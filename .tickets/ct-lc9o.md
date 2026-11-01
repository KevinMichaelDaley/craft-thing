---
id: ct-lc9o
status: open
deps: []
links: []
created: 2026-11-01T19:46:39Z
type: bug
priority: 2
assignee: kmd
tags: [physics, regression]
---
# Investigate baseline catapult cold-launcher support failure

Before beaver-player code changes, ./tools/with-vulkan make test at b262bf6f exited at Makefile:935 test_catapult_scene. tests/app/catapult_scene_tests.c:343 in test_cold_launcher_has_supported_frame_and_real_latch fails support.state<=FR_STRUCTURAL_OVERLOADED after release; 12 cases passed, 1 failed. Original baseline PID 5759 started 2026-11-01 19:29:04 UTC and completed around 19:45 UTC. Full log /tmp/craft-player-baseline-tests.log (archived under build/agents/beaver-player/baseline-tests.log). Investigate actual support transition without weakening assertions or disabling the test.

## Acceptance Criteria

Reproduce failure, explain support transition, fix under RED/GREEN/REFACTOR, and rerun catapult scene and full test suite.

