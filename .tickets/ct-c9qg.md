---
id: ct-c9qg
status: closed
deps: []
links: []
created: 2026-11-02T02:11:46Z
type: task
priority: 1
assignee: kmd
parent: ct-unkh
---
# Accumulate localized chewing work from authoritative wood mass

CPU-only prerequisite while user tests live demo: a caller-owned chew work budget keyed to actual parcel identity and generation. Explicit effort tuning consumes actual dry mass in Q16 units; denser or more occupied wood requires proportionally more effort. No GPU readback, invented RPG stats, wood deletion or inventory credit in this layer.

## Acceptance Criteria

Compiling RED then GREEN tests cover wood versus dense-wood shared physical profiles, occupied amount, cadence equivalence, stale/changed target and release handling, bounded stalls, invalid input transactional behavior. Return ready for a revalidated commit only; consumption/structural release and inventory are separate atomic GPU work and remain ct-unkh acceptance.


## Notes

**2026-11-02T02:13:49Z**

CPU-only implementation delivered: caller-owned single-parcel mass/work accumulator with explicit effort tuning, shared wood/dense-wood profile and occupied-amount tests, exact integer cadence independence, release pause, stale identity/generation/material/mass reset, overflow-safe stall saturation at one bite and successful-world-transaction acknowledgement. RED0/6 then GREEN6/0. dry_mass_fp input must be current fuel+char+inert stock, never reactive reference metadata. No GPU resources rebuilt and no world mutation/inventory credit claimed; ct-unkh atomic GPU work remains. CPU suite passes; full suite deferred per instruction while demo PID41100 open.
