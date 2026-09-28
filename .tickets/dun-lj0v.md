---
id: dun-lj0v
status: in_progress
deps: []
links: []
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

