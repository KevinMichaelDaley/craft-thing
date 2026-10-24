---
id: ctlo-y5ak
status: closed
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

**2026-10-24T19:36:06Z**

GREEN default and explicit reference smoke both pass. Replaced only the pre-step fixed-cell assumption with complete captured cell-plane equality and mass/live-particle byte equality on reload. Existing149-marker count, every face-velocity byte, halo edits, negative-coordinate reload and process-restart checks retained. No engine behavior changed.

**2026-10-24T19:44:49Z**

Complete Wayland test_ui rerun now passes every command, including the strengthened streaming persistence smoke. Quarter-native suite also passes all commands.
