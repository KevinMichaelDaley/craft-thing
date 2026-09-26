---
id: dun-u68k
status: open
deps: [dun-o1go, dun-cwhn]
links: []
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

