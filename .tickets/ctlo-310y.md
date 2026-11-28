---
id: ctlo-310y
status: closed
deps: []
links: []
created: 2026-10-24T07:10:15Z
type: task
priority: 1
---
# Use direct addresses for untagged solver contact metadata

## Acceptance Criteria

Contact rows and contact scratch use their known metadata address domain without decoding tagged per-body addresses. Keep tagged per-body state access intact. Frozen uncached solver agrees bytewise after growth; profile shows zero generic contact metadata resolutions; displayed performance required.


## Notes

**2026-10-24T07:15:46Z**

RED generic contact metadata resolutions54720; GREEN0 with exact body bytes after dynamic queue growth, actual sync validation1/1. Nonprofiled SOLVE kernel6085->4766 instructions,339->299 sends, no spills. Displayed measurement underway.

**2026-11-28T04:06:51Z**

Current full GPU suite verified contact metadata resolutions fell from 54720 to zero with exact full body records through dynamic growth. The post-change real-time Wayland tree-cut workload measured 32.414 ms mean and 38.887 ms worst over 500 frames (tools/diagnostics/README.md).
