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


## Notes

**2026-11-01T22:22:49Z**

Player-foundation full-suite rerun independently reproduced the identical tests/app/catapult_scene_tests.c:343 support-state assertion, 12 passed/1 failed, make exit 2. Log build/agents/beaver-player/state-full-tests.log. Player sources are isolated CPU modules; original catapult assertions unchanged.

**2026-11-01T22:39:10Z**

Third observed occurrence: final native-asset full ./tools/with-vulkan make test rerun exits 2 at the exact same support.state assertion at catapult_scene_tests.c:343, 12 pass/1 fail. Log build/agents/beaver-player/asset-full-tests.log. No catapult implementation or assertion changed.
