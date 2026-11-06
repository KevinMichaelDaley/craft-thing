---
id: ct-p2ea
status: open
deps: []
links: []
created: 2026-11-06T09:19:29Z
type: task
priority: 2
assignee: kmd
---
# Beaver fire pile fuel does not decrease in runtime demo

beaver_fire_runtime_tests FAIL at tests/app/beaver_fire_runtime_tests.c:130 pile_after.fuel<pile_before.fuel — the burning pile's fuel stat never drops across the demo window. Reproduced at 759c708a~1 (pre-existing from the ABI-8/thermal wave). Likely interaction with burn_coating viability max(skin,bulk) change or combustion push width; check whether pile fuel drains only when coating viability uses bulk energy.

