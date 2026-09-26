---
id: dun-u68k
status: open
deps: [dun-o1go, dun-cwhn]
links: [dun-wc20]
created: 2026-09-26T05:25:21Z
type: task
priority: 2
assignee: kmd
parent: dun-p9g9
tags: [gpu, materials]
---
# Implement deterministic granular and gas movement

Generate movement intents from immutable source cells, resolve destination conflicts, rotate lateral preference, and commit ping-pong output.

## Acceptance Criteria

Stable piles and rising gas behave consistently; no duplicated/lost particles in conflict or chunk-edge tests.


## Notes

**2026-09-26T07:43:58Z**

Scope update: retain gas and simple cell-rule motion here; dirt, sand, and gravel movement is specified by MPM ticket dun-wc20 and its particle storage. Avoid two authoritative granular solvers.
