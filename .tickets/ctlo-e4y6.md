---
id: ctlo-e4y6
status: in_progress
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


## Notes

**2026-11-01T09:56:53Z**

First RED-GREEN-REFACTOR increment e3e84cd, 04f647c and next refactor commit: CPU contact reference transfers exact Q16 mobile oil/slime volume and proportional excess heat to a stable terrain/rigid/leaf/actor rest-address film. Retained stock cannot wet; water is not converted to a combustible film. Five targeted tests and full make test under Xephyr :92 pass. Remaining work: runtime GPU contact admission, hot/cold film mobility, force/adhesion drip and tear, reactive parcel integration, fracture/stream handoff, water heat/drag coupling and displayed fixture.
