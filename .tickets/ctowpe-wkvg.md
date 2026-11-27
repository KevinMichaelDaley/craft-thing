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


## Notes

**2026-11-27T11:59:14Z**

Both controlled shutdown cases now pass on Xephyr :93. Slow actual save receipts extend the idle deadline, while withholding every receipt still reports the timeout. The original six smoke-box cases passed independently before adding the two regressions. Full make test is running.

**2026-11-27T13:48:19Z**

The 6fa25fe8 full-run log reached the final stream tests and contains zero reported test failures. The smoke enclosure and watchdog cases passed 8/8. The process wrapper returned 143 rather than a clean exit; the supervised 274d015c full-suite job is the outstanding verification.
