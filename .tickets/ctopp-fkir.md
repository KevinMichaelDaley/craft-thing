---
id: ctopp-fkir
status: in_progress
deps: []
links: []
created: 2026-11-27T05:49:25Z
type: task
priority: 1
assignee: kmd
parent: ctopo-yhkb
tags: [offscreen, streaming, physics]
---
# Project the shared liquid face with the native pressure solver

## Design

Stitch mapped neighboring tiles into a sealed joint projection domain and reuse the native hydro head and pressure passes. Preserve both live owner fields while sampling both endpoints and the shared normal.

## Acceptance Criteria

Compare horizontal and vertical sampled face normals and endpoint pressures with the native monolithic projection. Exercise positive and negative flow, full and fractional apertures, blocked faces, and invalid owners.

