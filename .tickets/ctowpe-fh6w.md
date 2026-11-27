---
id: ctowpe-fh6w
status: open
deps: []
links: []
created: 2026-11-27T19:16:55Z
type: bug
priority: 2
assignee: kmd
parent: ctowpe-t43r
---
# Diagnose missing older resident epoch in gas camera reopen regression

The full Xephyr suite at 8f26f761 failed test_camera_reopens_shared_gas_stock_and_pressure during cycle two because it found no active resident slot older than world time. The current fixture still has that precondition.

## Acceptance Criteria

Reproduce on the current revision, determine whether the fixture or epoch handling causes the failure, and verify shared gas and older resident clock behavior with the focused and aggregate suites.
