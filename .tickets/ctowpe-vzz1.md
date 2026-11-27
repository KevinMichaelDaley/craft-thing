---
id: ctowpe-vzz1
status: in_progress
deps: []
links: []
created: 2026-11-27T13:26:19Z
type: task
priority: 2
assignee: kmd
parent: ctcdo-rcs9
tags: [offscreen, streaming, physics]
---
# Carry MPM grains across a shared owner face during native particle substeps

## Design

Use the interior particle grid and ordered two-layer gather across the shared face. Preserve current liquid, rigid and granular coupling, particle IDs, material, moisture and associated reactive ownership. Feed the confirmed result into the existing paired owner checkpoint.

## Acceptance Criteria

Compare bidirectional crossing particles and receiving layers to one native world, conserve stock and momentum, preserve closed behavior when the neighbor is absent, and verify a camera handoff and cold reopen.

