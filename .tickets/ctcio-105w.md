---
id: ctcio-105w
status: in_progress
deps: []
links: [ctpco-qxpf, ctwi-7ukj]
created: 2026-11-26T22:36:30Z
type: bug
priority: 1
assignee: kmd
parent: ct-exwr
tags: [player, input, beaver]
---
# Cancel uncommitted casts when their input owner is lost

The latched spell survives controller disconnect or a round trip through editor, UI or window focus. A disconnected player can still alter the world, and a recovered focus or a replacement controller can deliver stale aim.

## Acceptance Criteria

Cancel only the relinquished owner’s pending cast before frame six. Preserve another seat’s cast, already published effects and recovery, and permit a fresh cast after ownership is regained. Exercise the live input path and both spell modes.


## Notes

**2026-11-26T22:38:51Z**

RED drove the real player tick through a UI round trip, disconnected device state, and lost focus. The old code emitted one stale storm after each of the first two and one ray while unfocused. All three regressions failed. Existing controls and already emitted recovery passed. Two native SDL scenarios compiled.
