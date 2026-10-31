---
id: ct-42nt
status: closed
deps: []
links: [ct-sqpj]
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


## Notes

**2026-10-31T20:41:34Z**

Implemented geometric open-sky loss and paired soot-flame/solid exchange with occlusion and finite source budgets. GPU tests: isolated 1400 K wood lost 3.696044 J per 0.05 s; gas-to-wood and blocker ledgers match within 0.0001 J. Displayed quarter-native bonfire passed twice: 8/8 dry and 0/8 wet at 45.1 s and 21.6 s, with gas peaks below plasma range. Repeated 1000-frame benchmark passed at mean 17.44/17.41 ms and worst 27.76/27.46 ms. Full make test passed on final refactored source.
