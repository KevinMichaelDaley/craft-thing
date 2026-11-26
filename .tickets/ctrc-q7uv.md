---
id: ctrc-q7uv
status: in_progress
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

## Notes

**2026-11-26T15:17:10Z**

RED moved 8002/3/5 Q16 oil, jelly and oxidizer in one second when called once and 32006/10/18 when called 60 times. The new elapsed-time law leaves 75 percent of the film after one continuous second. The Vulkan tests now move exactly 8002/3/5 at 1, 60 and 240 steps, with zero whole stock moved at the 1e-7-second probe. Reciprocal and competing offers preserve mass and enthalpy, and a live arena resize preserves fractional progress. Warm credits hold less than one Q16 unit per species, follow arena generations, and cold restores restart those credits at zero. Saved physical stock uses the existing format. Coating and residue checks, allocator rollback and address diagnostics passed. The launcher command now reads the parent fixture delay, threshold and all locked seeds directly.

**2026-11-26T15:25:12Z**

The completed coating target passed all eight Vulkan cases. The application douse preflight passed 3/3, the 30-second douse replay retained 45080/0 Q16 late-window consumption, and the clean pool corrosion replay matched gold .009047 and ingredient .565406. The exploratory launcher run passed the actual prompt release, fireproof carrier and the first two delayed seeds. It was ended to give the fixture-defined run the GPU slot. The active locked sweep uses all 64 JSON seeds, ten seconds, and 48 required ignitions. It passed the prompt and fireproof controls and seed 991397 ignited 17 arm parcels. Keep the ticket open until the locked sweep has a terminal result.
