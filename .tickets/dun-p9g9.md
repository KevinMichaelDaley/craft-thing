---
id: dun-p9g9
status: open
deps: []
links: []
created: 2026-09-26T05:25:20Z
type: epic
priority: 1
assignee: kmd
tags: [gpu, materials]
---
# GPU falling-sand and material-rule stages

Run ordered compute passes for material reactions, granular motion, gases, and cleanup after the fluid pass. Resolve competing writes deterministically.

## Acceptance Criteria

Sand forms stable piles without duplication or loss; chunk-edge moves work; conflicting moves/reactions have deterministic winners for a fixed device and shader build; stage order is validated.

