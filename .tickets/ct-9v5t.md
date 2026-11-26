---
id: ct-9v5t
status: in_progress
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

