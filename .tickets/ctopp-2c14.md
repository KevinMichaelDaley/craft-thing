---
id: ctopp-2c14
status: open
deps: [ctopp-fkir]
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
