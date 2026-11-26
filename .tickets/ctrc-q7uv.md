---
id: ctrc-q7uv
status: open
deps: []
links: []
created: 2026-11-26T14:48:24Z
type: bug
priority: 2
assignee: kmd
parent: ctlo-yvip
tags: [fire, coating, vulkan]
---
# Tie rigid coating adhesion to simulated contact time

The rigid coating publisher preserves the earlier quarter-of-stock share on every combustion call. The ownership regression demonstrates that the full quarter moves even on a 1e-7 second step. The launcher notes describe a per-second transfer. Splitting the same continuous contact across thermal steps currently changes the admitted film.

## Design

Measure sustained contact at multiple supported cadences and preserve the finite film and enthalpy ledger. Use the elapsed contact duration or an explicit contact transition for each deposit. Keep the launcher fixture and physical coating properties as the calibration evidence.

## Acceptance Criteria

Locked equivalent-time body contacts transfer the same film and heat within documented Q16 precision across supported cadences. One-writer ownership, prompt release, delayed ignition, and fireproof controls stay green.
