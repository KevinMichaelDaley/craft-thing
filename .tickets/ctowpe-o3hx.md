---
id: ctowpe-o3hx
status: in_progress
deps: []
links: []
created: 2026-11-27T20:35:47Z
type: task
priority: 1
assignee: kmd
parent: ctfm-w80b
---
# Keep offscreen marker page-table copies within the workspace slot allocation

Marker initialization and chunk remapping copy the full resident CPU reverse-page array even when the mapped destination was allocated for a smaller workspace.

## Acceptance Criteria

Initialize, replace and unmap compact reverse-page tables without touching trailing words. Verify resident-sized tables and retain the existing page and velocity semantics.


## Notes

**2026-11-27T20:37:36Z**

Both compact cases originally failed and the resident table passed. Bounded both copies to the allocated slot count and verified all three page-table cases, including an unaffected third mapped slot through replacement and removal. Both fluid recording tests and make test_tools passed. The aggregate uses the hidden Xephyr display.
