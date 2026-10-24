---
id: ctlo-y5ak
status: in_progress
deps: []
links: []
created: 2026-10-24T19:34:24Z
type: bug
priority: 2
---
# Diagnose streamed smoke reload state mismatch

## Acceptance Criteria

Explain which saved state differs on positive reload, compare default and explicit reference modes, and preserve exact persisted state assertions without assuming a moving grain remains in its painted cell.


## Notes

**2026-10-24T19:35:41Z**

Default and explicit all-reference flags both reproduce failure. Diagnostic RED: reloaded material0, captured post-step material0, original hardcoded expectation2; marker149/149 and every face-velocity byte equal. Strengthened smoke snapshots the complete post-step cell plane and all live particle bytes; retain old fixed-cell assertion in RED to expose its stale assumption.
