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

**2026-11-01T10:10:06Z**

Second RED-GREEN-REFACTOR increment 63dcd82, d8d8b59 and current refactor: profile adhesion/viscosity release finite oil or slime film into receiving shared liquid capacity. Cold film holds; hot film drips; intermediate temperature softens continuously. Full receiver keeps film stock; mixed film heat partitions by phase heat capacity and energy is retained. Nine focused tests, generated-material dependency check and full make test under Xephyr :92 pass. Runtime GPU attachment, parcel/combustion ledger mapping, fracture and streaming handoff remain.

**2026-11-01T10:13:25Z**

Third RED-GREEN-REFACTOR increment dea1a1c, d8d154f and current refactor: deterministic Q16 fracture split assigns each oil/slime quantum exactly once to two distinct stable rest-address owners; heat is apportioned by phase heat capacity and conserved. Rejects duplicate child owner without touching output. Twelve focused tests and full make test under Xephyr :92 pass. Runtime GPU contact admission, combustion linkage and streaming film persistence remain.
