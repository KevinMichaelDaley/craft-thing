---
id: ctr-dp6j
status: closed
deps: []
links: []
created: 2026-10-17T10:37:35Z
type: bug
priority: 1
---
# Start imported scene gameplay with physics running

## Acceptance Criteria

Scene demo starts unpaused; P still toggles pause.


## Notes

**2026-10-17T10:39:18Z**

Scene startup now initializes paused=false; P toggle unchanged. Quarter-native demo builds cleanly. Full test rerun stopped at user request because change is minimal; previous suite results remain recorded. Documentation updated.
