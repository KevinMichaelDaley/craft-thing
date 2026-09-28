---
id: dun-bd93
status: open
deps: [dun-wwcl]
links: []
created: 2026-09-28T02:43:11Z
type: task
priority: 1
assignee: kmd
parent: dun-0t9m
---
# Conserve water and grains across offscreen cadence boundaries

Different-distance chunks have different due times. Add GPU equal-and-opposite face flux and particle handoff/ledger so sleeping chunks do not become walls and promotions apply pending transfers before rendering.

## Acceptance Criteria

Mass, particle IDs and momentum remain stable across active/offscreen boundaries and camera promotion; no artificial boundary pool or missing pixels in long pan regression.


## Notes

**2026-09-28T16:31:34Z**

RED integration test proves supported water on an offscreen chunk cannot cross into an adjacent visible chunk after 120 GPU ticks; boundary remains a false wall. GPU-only cross-context handoff required.
