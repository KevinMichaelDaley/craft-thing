---
id: ctoae-ks83
status: closed
deps: []
links: []
created: 2026-11-27T04:28:51Z
type: bug
priority: 1
assignee: kmd
tags: [build, player, reactive]
---
# Rebuild the storm footprint test after coating layout changes

The existing GPU dependency regression finds that the storm footprint binary compiles reactive_state.c without depending on coating_layout.h. Full test runs stop at this check.

## Acceptance Criteria

The GPU dependency suite passes and make schedules a rebuild of the storm footprint binary when the coating header changes.


## Notes

**2026-11-27T04:29:22Z**

Reproduced on the ancestor full-suite revision 1518d1f1 and on the active-election branch before changing the Make rule. After adding the header dependency, make test_gpu_tools and all six storm-footprint cases passed. make -n -W include/dungeoncraft/coating_layout.h build/player_storm_footprint_tests emitted the compile and link command.
