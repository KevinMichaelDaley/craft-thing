---
id: ctpc-xa7p
status: in_progress
deps: []
links: []
created: 2026-11-26T15:28:45Z
type: bug
priority: 1
assignee: kmd
parent: ctsl-7ay0
tags: [fire, coating, vulkan]
---
# Keep mixed coating debits proportional at simulation cadence

Each one-unit reaction rounds the oil share down, charging the entire debit to the jelly. Repeated fine steps deplete the jelly early and abandon oil while the host is submerged and bound oxidizer remains.

## Design

Round the proportional fuel split to the nearest representable whole unit, clamped to available species. Preserve the existing reaction rate and complete fuel, oxidizer, product and enthalpy accounting.

## Acceptance Criteria

At the 60 Hz simulation cadence, a submerged balanced film continues consuming both species when reaction steps admit only one whole unit. CPU reference and Vulkan record the same species balances and close their finite product ledger.

