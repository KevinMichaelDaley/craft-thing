---
id: ct-w5rq
status: closed
deps: []
links: [ct-li7d]
created: 2026-10-31T22:37:35Z
type: bug
priority: 1
assignee: kmd
parent: ctfs-81vw
tags: [fire, thermal, gpu]
---
# Resolve standalone body hot-skin energy budget failures

While reproducing ct-li7d on X11/RTX A2000, combustion_body_feedback_display_tests had two existing failures before the char implementation: rotated mixed temperature and hot skin both reported negative or large total energy drift in hot_skin_budget_case. The test target is outside make test, so investigate the sample/flux ledger and fixture accounting separately.

## Acceptance Criteria

Run the full standalone combustion_body_feedback_display_tests on this GPU; both hot-skin cases conserve solid/gas energy within their stated tolerance and preserve nonnegative parcel energy; no regression to bonfire ignition or make test.


## Notes

**2026-11-02T15:35:29Z**

Verified on this machine (NVIDIA 580.178.04, X11, quarter-native flags): build/combustion_body_feedback_display_tests completes 11 passed, 0 failed. Both hot-skin cases conserve energy exactly: rotated mixed temperature body 377.762702J total drift 0J; hot skin body 485.999436J total drift 0J; parcel energies nonnegative. Headless sibling passes 10/1 (only presentation-focus test needs a window). Bonfire ignition unaffected: bonfire_display_tests exits 0 with kindling igniting (7 cells) and logs igniting (1224-1649K skin), bonfire progress/scene/shutdown all pass. make test: single remaining failure is body_carve_tests large-brush latency 39.479ms vs 30ms bound on this GPU, which open ctlo-jnt7 owns (pre-existing machine timing, no runtime code changed). Session repairs required to run the recipe: glslangValidator+spirv-val installed to ~/.local/bin (apt download + dpkg -x, no sudo available); gpu_dependency_tests player_chew missing material_profile_data.inc dep fixed; bonfire_display_tests/thermal_bench_display_tests link rules repaired for player_session. Known environment pathology: NVIDIA libnvidia-glvkspirv spins minutes per new pipeline on cold runs (combustion display run needed >15min wall); buoyancy_tests needed ~15-30min. Documented follow-ups: ct-2cad (3 combustion_gpu_display failures).
