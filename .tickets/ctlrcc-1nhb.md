---
id: ctlrcc-1nhb
status: open
deps: []
links: []
created: 2026-11-27T01:07:02Z
type: task
priority: 1
assignee: kmd
parent: ctfm-w80b
tags: [world-clock, checkpoint]
---
# Bind structure checkpoint chunks to their saved simulation times

The level may restore physical chunks from the embedded structure checkpoint before the stream packet. This version has no clock for that stock, so level residency intentionally leaves its timestamp unknown. Save and restore timestamps as part of the embedded chunk snapshot.

## Acceptance Criteria

Persist a completed time with the exact embedded physical chunk and transfer it into the matching level generation after checkpoint publication. Preserve prior DCS layouts and leave their timestamps unknown. Verify agreement with staged world recovery, pending checkpoint chunks and reopened worlds.


## Notes

**2026-11-27T01:07:03Z**

This closes the known gap between worker-only cold return and the structure-checkpoint override used for ordinary world reopen. Bind the clock to the source that supplied physical stock.
