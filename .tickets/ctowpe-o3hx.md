---
id: ctowpe-o3hx
status: closed
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

**2026-11-27T20:49:03Z**

The full native phase trace on 469573d1 also completed all 180 presented frames after the compact table fix. It retained 16 confirmed owners and advanced 2.000000104 s in every band. The two compact allocation tests and resident control pass; full-suite status remains tracked by ctfm-w80b.
