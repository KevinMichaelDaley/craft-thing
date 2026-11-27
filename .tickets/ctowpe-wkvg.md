---
id: ctowpe-wkvg
status: in_progress
deps: []
links: []
created: 2026-11-27T11:48:57Z
type: bug
priority: 1
assignee: kmd
tags: [streaming, save, testing]
---
# Investigate smoke-box shutdown timing out during the full graphical suite

## Design

The full Xephyr make test run reached the smoke enclosure suite and failed while closing the roof-vent case. Five of six passed. Reproduce using the suite-defined smoke profile and trace acknowledged saves before changing the watchdog.

## Acceptance Criteria

A progressing shutdown persists every dirty owner even when readback and disk acknowledgments span the previous overall timeout. A genuinely stalled or rejected owner must still fail. Re-run the smoke suite and full test target.

