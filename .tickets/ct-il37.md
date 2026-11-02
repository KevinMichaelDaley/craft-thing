---
id: ct-il37
status: in_progress
deps: []
links: []
created: 2026-11-01T19:47:37Z
type: bug
priority: 1
assignee: kmd
tags: [build, tests]
---
# Restore full default test recipe hidden by thermal display rule

Existing Makefile defines recipes twice for test: the long default battery near original line 720 is overridden by a thermal_phase_gpu_display_tests recipe near original line 2112. GNU make warns overriding recipe/ignoring old recipe. Prerequisites still build, but most default battery executables are not run. Observed during untouched baseline at b262bf6f and player tests. Keep all checks; use a dedicated phony target for thermal display rather than replacing the default test recipe.

## Acceptance Criteria

Regression proves make test retains the full default run recipe and includes thermal display acceptance without override warnings. Full suite is attempted with failures triaged.


## Notes

**2026-11-02T15:35:35Z**

Follow-up from restored recipe: gpu_dependency_tests flagged build/player_chew_tests (from ct-c9qg) missing src/fire/material_profile_data.inc prerequisite; fixed in tests/player/player.mk using the beaver_asset_tests pattern. Full recipe also exposed broken bonfire/thermal_bench display link rules (fixed under ct-8h2f) and needs glslangValidator+spirv-val on PATH (installed to ~/.local/bin on this machine).
