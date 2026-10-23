---
id: ctlo-e81b
status: closed
deps: []
links: []
created: 2026-10-23T14:48:57Z
type: task
priority: 1
parent: ctr-ebl5
tags: [app, renderer]
---
# Make propagated lighting an opt-in gameplay flag

User directs lighting to be gated for now. Default scene gameplay must use authored material colors without propagated-light work. Add --lighting to opt in, retain explicit renderer tests and lighting toggle.

## Acceptance Criteria

Actual Wayland default and --lighting launches load level03, preserve stable paused frames and produce distinct albedo/lit captures; default skips propagation. Regression observes missing flag/default policy before implementation. Full suite and push.


## Notes

**2026-10-23T14:51:56Z**

RED actual-window regression failed because --lighting was unsupported. GREEN default and opt-in launches pass tests/app/lighting_flag_tests.c; inspected both real captures in build/lighting_flag_Xh9J4x. Default retains authored colors, opt-in shows propagated shadows; each has four identical paused captures. README documents --lighting and F8. Final full suite running; propagated performance target deferred at user direction, no strict benchmark gates weakened.

**2026-10-23T14:54:57Z**

Final full make -j4 -i test completed: build/lighting_flag_full_suite.log has exactly the two pre-existing ctr-ojvy authored_stage failures67/135, zero new failures or warnings. Graphical default/--lighting regression1/1 and dependency suite3/3 pass. Source injection, field propagation, renderer correctness and strict performance gates remain available unchanged for explicit future lighting work. Build-rule placement preserves the existing default make target.
