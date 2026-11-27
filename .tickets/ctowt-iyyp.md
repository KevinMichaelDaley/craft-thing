---
id: ctowt-iyyp
status: closed
deps: [ctopp-fkir]
links: []
created: 2026-11-27T06:19:35Z
type: task
priority: 1
assignee: kmd
parent: ctopp-2c14
tags: [offscreen, streaming, physics]
---
# Capture native water sweep crossings and final paired stock

## Design

Run the native directional transport on isolated paired GPU scratch. Record each original donor and receiver across the current seam, retain expected and final stocks, and derive signed face pressure work from the same projection.

## Acceptance Criteria

Native split/monolithic comparisons are exact on water stock for both axes and cover both flow directions, distant recipients, a shallow aperture, a blocked seam, bulk receiver backpressure, and read-only rejection.


## Notes

**2026-11-27T06:32:48Z**

The real GPU split and monolithic basins match for every Q16 stock, net face transfer, and native pressure work under both default specialized and cached/generic long-scan configurations. Four donors per direction, distant recipients, partial and blocked apertures, oil backpressure, stale binding, and deterministic re-planning pass with synchronization validation. The computation is isolated from the live owners. Carried fields and durable owner scheduling have their own dependent tickets.
