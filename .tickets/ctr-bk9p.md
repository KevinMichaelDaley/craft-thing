---
id: ctr-bk9p
status: closed
deps: []
links: []
created: 2026-10-18T01:17:27Z
type: bug
priority: 1
---
# Keep cut tree components fixed while terrain soil still supports them

## Acceptance Criteria

A trunk nick must not extract a soil-supported loaded component. A severed unsupported crown extracts while the supported stump remains terrain.


## Notes

**2026-10-18T01:34:45Z**

Loaded edit markers now request support reevaluation instead of unconditional extraction; ordinary nonloaded solids retain explicit release. RED regression reproduced supported-stump extraction. GREEN dense support 7/7 and sequential detachment 5/5; actual level03 buried-trunk graphical regression 1/1 under Vulkan validation. Full make test completed with only the two pre-existing authored_stage_tests failures tracked in ctr-ojvy. Loose crown soil support follow-up ctr-9e4n. Existing live demo was SIGSTOP paused for serial GPU validation and resumed without closing or losing edits; it needs normal close/relaunch to load changed pipelines.
