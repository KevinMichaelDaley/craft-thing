---
id: ct-p2ea
status: in_progress
deps: []
links: []
created: 2026-11-06T09:19:29Z
type: task
priority: 2
assignee: kmd
---
# Beaver fire pile fuel does not decrease in runtime demo

beaver_fire_runtime_tests FAIL at tests/app/beaver_fire_runtime_tests.c:130 pile_after.fuel<pile_before.fuel — the burning pile's fuel stat never drops across the demo window. Reproduced at 759c708a~1 (pre-existing from the ABI-8/thermal wave). Likely interaction with burn_coating viability max(skin,bulk) change or combustion push width; check whether pile fuel drains only when coating viability uses bulk energy.


## Notes

**2026-11-07T19:34:17Z**

Same two ABI-8 strides; the pile fuel never drained because the scene's checkpoint restore was loading a stride-garbled reactive archive, leaving the pile unlit. beaver_fire_runtime_tests now 1/0 with real pile drain (330304800 -> 330294862, char 1961, flaming 67).
