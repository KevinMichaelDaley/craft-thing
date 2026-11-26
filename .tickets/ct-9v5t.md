---
id: ct-9v5t
status: closed
deps: []
links: []
created: 2026-11-26T05:42:31Z
type: bug
priority: 1
assignee: kmd
parent: ct-exwr
tags: [player, beaver]
---
# Replace prototype placement and cast shortcuts with owned player actions

The playtest help promises inventory-backed R/B building and select-then-cast, but R still spawns an untyped generic block under a reused 16-tick ID, B is unhandled, and selecting a spell updates the event after dispatch. Wire live input through the hotbar, one-shot animation events, and transactional material authoring.

## Acceptance Criteria

Each controller/player owns selection and cast target. Cast once at the authored frame. Successful placement consumes exact selected material mass. Rejected or retried placement changes neither inventory nor world. Document only controls exercised by runtime tests.


## Notes

**2026-11-26T06:23:26Z**

RED reproduced ignored controller selection, firing the wrong spell after selecting ice, unpaid rigid admission, and missing terrain placement. The live session now resolves the owned slot before input arbitration and dispatches the spell and target latched at windup on frame six. R places one authored terrain cell and B places a typed 4x4 convex body using a free runtime ID. Validated all four live action cases, including two paid bodies with distinct IDs, a blocked footprint with sufficient stock, a material mismatch, and the solver mass after a real tick. Input, SDL, hotbar, player runtime, split-screen, brush, rigid carve, default recipe, and dependency checks passed. Both pattern shader variants validate.

**2026-11-26T06:24:15Z**

Final follow-up passed all six convex-body cases and the actual fire-ray demo, including 330 live ticks, pile ignition, and exact gas reload. The ongoing broad suite and its queued rerun remain tracked under ct-il37. The fire-bolt admission defect remains tracked under ct-q3hh.
