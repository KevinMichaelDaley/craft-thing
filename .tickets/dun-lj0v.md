---
id: dun-lj0v
status: closed
deps: []
links: [dun-1c2f]
created: 2026-09-28T01:28:19Z
type: bug
priority: 1
assignee: kmd
parent: dun-9e5b
---
# Prevent upward water spray after impacts

The half-native demo still shows many water pixels bouncing above the basin after the marker-slot fix. Compare identical impact runs with marker correction enabled and disabled, then remove the nonphysical source of persistent upward spray without suppressing real velocity-driven motion.

## Acceptance Criteria

A spring-off impact settles without persistent high airborne water; marker-on/off diagnostics identify the contributing pass; mass conservation, fast lateral propagation, and full headless/UI suites pass.


## Notes

**2026-09-28T02:09:10Z**

The 960x540 spring-off smoke samples 12 frames across 120 ticks after 1,200 spring ticks. At six-tick fluid steps, marker correction disabled still averaged 41.8 high airborne pixels (peak 95). At three-tick steps and 16 pressure sweeps, marker-disabled averaged 2.8 (peak 26) and marker-enabled averaged 1.5 (peak 16). The large integration step, not marker correction, is the spray source. Native six-tick stability is tracked in dun-1c2f.
