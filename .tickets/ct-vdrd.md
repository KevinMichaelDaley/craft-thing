---
id: ct-vdrd
status: open
deps: []
links: []
created: 2026-10-30T18:32:14Z
type: task
priority: 1
assignee: kmd
parent: ctfs-5gil
tags: [vulkan, performance, rigid]
---
# Reduce repeated rigid solver work in sparse bonfire scenes

RTX A2000 half-native bonfire frame trace shows rigid GPU about 11ms, with position solve/apply dominating. Quarter detailed trace measured phase6 solve ~1.8ms and phase7 apply ~1.3ms across 8 substeps and 8 iterations. Existing fused-grain option worsened half-native mean to35.130ms. Profile exact active contact/body work and cut redundant dispatch/barrier cost without changing physical clock, substep count, or contact solver results.

## Acceptance Criteria

Focused solver/catapult/bonfire physics tests pass with zero validation errors; displayed half-native 32-frame normal-clock timing improves mean and worst on this GPU; no fixed body counts or numeric-policy shortcuts.


## Notes

**2026-10-30T18:38:19Z**

Half-native detailed trace (diagnostic command reuse OFF) found phase6 solve2.65ms, phase7 apply0.85ms per frame, contact kernels ~8.7ms with instrumentation; coarse trace (reuse ON) rigid11.2ms total, solver6.7-7.0ms, contacts0.34-0.58ms, renderer1.6-1.75ms. Fused-grain32-frame mean35.130ms, grouped+skip-zero36.556ms (worse); skip-zero alone31.752ms mean35.610ms worst vs default32.467/41.708, still misses <30. Temporary half trace build target reverted; quarter trace tool already present. Need exact solver work reduction without changing substeps/clock.

**2026-10-30T19:12:03Z**

Trial fused CLEAR into PREPARE: focused rigid/command/grain tests passed, half-native 32-frame runs 35.762/42.586ms and 32.021/41.994ms mean/worst versus baseline32.467/41.708; no reliable gain, reverted. Trial skipped empty-grain phases based on mapped particle counts: grain work3/3 passed but rigid_solver_tests deterministically failed streamed-support and reproducible-stack exact-state cases (11/13); reverted. Solver remains open pending a semantics-preserving optimization.

**2026-10-30T20:56:55Z**

2026-10-30 late 2000-frame instrumented bonfire trace: rigid GPU increased from mean16.03ms at frames100-400 to17.84ms at1850-2000; thermal ~7.46->7.71ms. Gas GPU stayed ~14.1ms. This is trace-mode overhead, not uninstrumented acceptance. No solver change made; thermal512, gas line512, skip-empty gas trials were slower and reverted/disabled. Ticket remains open.
