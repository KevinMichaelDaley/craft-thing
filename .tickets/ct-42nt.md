---
id: ct-42nt
status: open
deps: []
links: []
created: 2026-10-31T06:54:24Z
type: bug
priority: 2
assignee: kmd
tags: [fire, radiation, vulkan]
---
# Couple open-sky radiation to flame and solid view factors

A focused GPU test shows isolated 1400 K wood should debit about 3.7 J per 0.05 s to open-sky Stefan-Boltzmann radiation. Adding that sink alone leaves the displayed bonfire at 2/8 dry logs because the current radiative pass couples solids but not luminous flames or hot gas across open cells. Implement reciprocal flame-to-solid radiative transfer and correct open-sky view factors as one energy-accounted change; do not ship the sink alone.

## Acceptance Criteria

RED tests for flame-to-solid energy transfer, open-sky loss, shadowing, and total energy ledger; GREEN no temperature clamp, displayed quarter-native 8/8 dry and 0/8 wet on repeated runs with plausible gas peak and full make test.

