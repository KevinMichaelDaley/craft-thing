---
id: ctlo-e4y6
status: open
deps: [ctlo-2vcr]
links: []
created: 2026-10-24T22:06:19Z
type: feature
priority: 1
parent: ctlo-d7vm
tags: [fire, smoke, physics]
---
# Attach transfer and melt reactive surface coatings

Transfer finite bulk oil/jelly to terrain, rigid surfaces, foliage and moving damageable-actor proxies through contact wetting; preserve rest-coordinate attachment and composition.

## Design

Follow design/fire_smoke_reactive_materials.md. Runtime implementation must use RED-GREEN-REFACTOR and repository tests/diagnostics; this turn creates the roadmap only.

## Acceptance Criteria

Bulk loss equals film gain; hot films spread/melt, cold films resist, and force/adhesion can cause finite drips or tears. Films follow rotating hosts, fracture and streaming without duplication or world-space smearing. Water changes heat/drag without deleting coating inventory. Tests include leaves and actor proxies with stable IDs/generations; no new monster AI or global object scan.

