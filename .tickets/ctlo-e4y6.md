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

**2026-11-01T10:27:59Z**

Runtime oil contact RED 8876389, GREEN 041a7cb and current refactor: existing GPU body wetting dispatch atomically reserves mobile oil from the adjacent bulk cell, credits profile-density Q16 oil mass to a rest-piece reactive parcel, and transfers proportional excess plus ambient thermal energy. Quarter-native Xephyr GPU case measured body +248 Q16 mass versus bulk -225 Q16 volume (oil density 1.1), with water in the contact cell and coating retained after body rotation. Seven focused GPU tests and full make test under Xephyr :92 pass. Display validation reports only older WSI enum/structure VUIDs, with no new dispatch or descriptor VUID. Remaining slime contact, terrain/leaf/actor, film melt/drip runtime, fracture/streaming handoff.

**2026-11-01T10:55:03Z**

Runtime jelly contact RED 154ee51, GREEN f81c213 and current refactor: GPU per-rest-piece contact debits mobile oil and red-slime jelly from shared-capacity bulk channels, credits oil fuel plus jelly fuel/bound oxidizer at profile density, moves corresponding heat, and leaves water in the same contact cell. Bulk activity bit skips coating scans in worlds without oil/jelly. Eight focused quarter-native Xephyr thermal GPU tests and full make test under Xephyr :92 pass. Outstanding: sub-Q16 volume-to-mass conversion remainders, runtime hot film release/drips, terrain/foliage/actor attachment, fracture/streaming owner transfer and displayed coating stress budget.
