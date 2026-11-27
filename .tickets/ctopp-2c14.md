---
id: ctopp-2c14
status: in_progress
deps: [ctopp-fkir, ctowt-iyyp, ctowt-7388]
links: []
created: 2026-11-27T06:02:05Z
type: task
priority: 1
assignee: kmd
parent: ctopo-yhkb
tags: [offscreen, streaming, physics]
---
# Derive signed water seam stock from native transport

## Design

Use the shared projected face and the existing water sweep with the same travel, aperture, capacity and Q16 arithmetic as an interior face. Retain contributions from all donor cells that reach the seam and apply the same receipt and durable owner transaction.

## Acceptance Criteria

Measure stock and face pressure work for horizontal and vertical split basins against a monolithic step, including both flow directions, a shallow face, receiver backpressure, multi-cell travel and rejection without stock changes.


## Notes

**2026-11-27T06:03:29Z**

fluid_transport.comp computes requests across multiple recipients before debiting a donor. A crossing packet can debit a donor behind the boundary and credit a receiver beyond it. Preserve the true donor and recipient locations in the paired write set, with signed per-face totals available for conservation and pressure-work checks.

**2026-11-27T06:30:47Z**

The paired scratch result includes intra-owner transport and crossings in the same sweep. A production commit must own that sweep at a common interval start. Applying only cross-face deltas after independent sweeps or applying overlapping pair results to a common tile would change the native result.
