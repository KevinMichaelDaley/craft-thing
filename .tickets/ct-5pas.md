---
id: ct-5pas
status: open
deps: []
links: []
created: 2026-11-01T01:49:00Z
type: bug
priority: 2
assignee: kmd
parent: ctlo-h9of
tags: [fire, renderer]
---
# Restore transported flame roof display test output

The quarter-native combustion GPU --roof scenario has finite soot (3.68734735e-05) but readback is uniformly magenta, so the red flame pixel assertion fails. Reproduces with both current shader and the pre-drying shader from 7d3e8da; it is not caused by ctlo-18l4 evaporation. Investigate presentation/renderer setup and preserve physical soot/flame assertions.

## Acceptance Criteria

At quarter-native display resolution, the roof/vent case renders a physical red flame when transported hot soot and oxygen are present, and all three roof/vent scenarios pass with no debug rendering. Re-run the current and baseline repro to verify the cause, then run full combustion display coverage.

