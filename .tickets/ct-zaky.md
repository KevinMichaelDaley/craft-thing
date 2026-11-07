---
id: ct-zaky
status: in_progress
deps: []
links: []
created: 2026-11-06T09:19:29Z
type: task
priority: 2
assignee: kmd
---
# Catapult scene structure save fails twice in scene tests

catapult_scene_tests 11 passed 2 failed: line 372 'saved' and line 398 fr_gpu_save_structure(g, build/catapult_released.dcs) fail. Reproduced at 759c708a~1 (pre-existing). Investigate whether structural save validation rejects the current deck/mast member set after the recent structural transaction changes.


## Notes

**2026-11-07T19:34:17Z**

Two independent defects: (1) the reactive export stride above; (2) FR_ARENA_REQUEST_INDEX=38 collided with the ABI-8 record's last word (state word 28/rot_exposure) so the arena shader's output handle write stomped restored states. Moved INDEX/GENERATION to 39/40, OLD_OWNER/RESERVED to 41/42, REQUEST_WORDS 43->44, FR_ARENA_GPU_ABI 2->3, rebuilt all 171 shaders. catapult_scene_tests 13/0 bit-exact roundtrip; player_runtime 1/0; beaver_fire_runtime 1/0.
