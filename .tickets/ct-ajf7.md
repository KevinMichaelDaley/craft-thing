---
id: ct-ajf7
status: closed
deps: []
links: []
created: 2026-10-30T17:18:32Z
type: bug
priority: 1
assignee: kmd
tags: [vulkan, test, solver]
---
# Align solver motion diagnostic descriptor bindings with shader

Fresh full-suite rebuild on NVIDIA validates binding 25 in solver_motion_profile.comp.spv, but solver_motion_store_tests declares only 12 descriptor bindings and segfaults after validation errors. Other solver fixture tests already bind 25.

## Acceptance Criteria

Focused solver motion, contact-start and grain-fusion tests pass with validation; full make test passes.


## Notes

**2026-10-30T17:20:36Z**

Fresh NVIDIA validation exposed missing binding 25 in the motion diagnostic fixture. RED explicit binding-count gate failed; GREEN binds 25 and corrects descriptor writes. Focused motion, contact-start and grain-fusion tests all pass with validation; full regression rerunning.

**2026-10-30T17:26:45Z**

Second full-suite rebuild found component_dispatch_tests omitted bindings 19,20,22,33,34,35 for component_extract_probe.spv. RED contract guard failed both extraction cases cleanly, GREEN bound the six required slots; focused component test 2/2 passes under NVIDIA validation. Full regression restarted.
