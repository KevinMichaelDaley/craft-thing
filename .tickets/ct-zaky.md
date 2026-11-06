---
id: ct-zaky
status: open
deps: []
links: []
created: 2026-11-06T09:19:29Z
type: task
priority: 2
assignee: kmd
---
# Catapult scene structure save fails twice in scene tests

catapult_scene_tests 11 passed 2 failed: line 372 'saved' and line 398 fr_gpu_save_structure(g, build/catapult_released.dcs) fail. Reproduced at 759c708a~1 (pre-existing). Investigate whether structural save validation rejects the current deck/mast member set after the recent structural transaction changes.

