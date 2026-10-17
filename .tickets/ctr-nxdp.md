---
id: ctr-nxdp
status: closed
deps: []
links: []
created: 2026-10-17T18:51:55Z
type: feature
priority: 1
---
# Retain detached authored pixel colors and save/load all color channels

## Acceptance Criteria

Custom RGB stays attached to body rest pixels through extraction, arena relocation and fracture. Versioned checked saves restore terrain/body/water colors, including chunk eviction/reload. Reopening scene does not stamp over saved edits. Legacy saves load safely; malformed color payloads fail atomically.


## Notes

**2026-10-17T19:22:58Z**

Implemented GPU rest-pixel RGB transfer across extraction, arena compaction and multiple same-tick fractures; checked version 6 snapshots/chunks preserve terrain/body/water HDR RGB. Reopen avoids asset restamping and explicitly initializes renderer. Focused validation: terrain_prop 4/4, body_color 1/1, structural_store 8/8, component_world 3/3, stream 7/7; Vulkan core/synchronization validation clean. Full suite running.

**2026-10-17T19:26:57Z**

Full make -j4 -i test completed: only the two pre-existing authored_stage_tests failures (component-plan getter and absent-normal override, ctr-ojvy); no new failures. Core and synchronization validation focused runs clean. Existing original saved world untouched.
