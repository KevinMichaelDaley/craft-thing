---
id: dun-u68k
status: closed
deps: [dun-o1go, dun-cwhn]
links: [dun-wc20, dun-vz9y]
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

**2026-09-30T05:18:04Z**

Implemented gas material 6 as deterministic GPU intent/winner/commit pass after MPM. Gas rises, alternates lateral preference, conserves count under conflicts and resident chunk-edge crossing; edge masks request streaming neighbors. Interactive key 6 and before/after one-second screenshot smoke added. Same-context GPU comparison at 128x64: granular stage idle 0.118 ms, gas active 0.133 ms. make test, make test_ui, make test_quarter_native pass. Independent workspace-to-workspace gas handoff remains in linked dun-vz9y.
