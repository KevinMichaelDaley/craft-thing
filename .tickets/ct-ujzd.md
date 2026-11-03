---
id: ct-ujzd
status: open
deps: []
links: []
created: 2026-11-03T17:36:20Z
type: bug
priority: 1
assignee: kmd
parent: ctlo-d7vm
tags: [build, make]
---
# Default make test recipe is clobbered by orphaned recipe lines under test: test_shock_exposure

Makefile lines near 'test: test_shock_exposure' carry orphaned recipe lines (mkdir -p build; $(CC) ... tests/fire/reactive_state_tests.c ... -o $@ -lm) that attach to the aggregated 'test' target itself. Consequences observed this session: (1) 'make test' compiles tests/fire/reactive_state_tests.c into a stray ./test ELF at the repository root instead of build/reactive_state_tests, and the intended per-test recipes for the aggregated target never run as written; (2) GNU make emits the 'overriding recipe for target test / ignoring old recipe' warning pair on every make invocation, which fails tests/build/default_test_recipe_tests.c (asserts no overriding-recipe warnings in make -n test) - verified pre-existing by running that binary inside a clean worktree at 67a490a0; (3) tests/vulkan/gpu_dependency_tests.py reports four dependency-lint failures at baseline (coating_layout consumers, direct-source gpu.c binaries, material coefficient consumers, structural carve modules - worktree-verified at 67a490a0). The defect predates this session (the orphan lines trace back to the dun-kjp9 era Makefile). Repair suggestion: move those two recipe lines onto build/reactive_state_tests (test_reactive_state's real target) and give the aggregated test: chain a prerequisite-only form, then re-baseline tests/build/default_test_recipe_tests.c and the dependency lint.

## Design

Edit the Makefile region around 'test: test_shock_exposure': the recipe lines belong to test_reactive_state (they compile tests/fire/reactive_state_tests.c into build/reactive_state_tests via -o $@ with .PHONY test_reactive_state already declared). After the repair, 'make -n test' must show no overriding-recipe warnings and must not emit a cc line writing -o test; ./test must not reappear at the repository root. Then run make -i test end to end and record the residual failures (expected: the documented ctlo-jnt7 body-carve latency failure and the four pre-existing dependency-lint failures if those rules genuinely lack the declared prerequisites).

## Acceptance Criteria

make -n test contains zero overriding-recipe warnings; no repository-root ./test binary is produced by make test; tests/build/default_test_recipe_tests passes; tests/vulkan/gpu_dependency_tests.py reports only genuinely-missing prerequisites if any remain, with the four baseline lint failures either fixed or explicitly waived in the lint itself; the full make -i test completes and its residual failures are each attributed to an open ticket.

