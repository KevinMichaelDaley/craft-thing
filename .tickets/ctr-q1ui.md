---
id: ctr-q1ui
status: closed
deps: []
links: []
created: 2026-10-17T21:28:22Z
type: feature
priority: 2
---
# Add cohesive leaves that crumble into light debris when broken

## Acceptance Criteria

Distinct leaves and leaf debris materials; supported canopy holds; mixed component extraction and save/load retain leaf material and colors; break via carve/impact produces light independent debris using parallel GPU emission.


## Notes

**2026-10-17T21:36:21Z**

Implemented terrain leaves=9, body leaves=5, debris=10, density 0.2. Parallel per-pixel particle reservations/emission reuse transactional remnant publication and roll back on exhaustion. Carve, impact capture, fluid/MPM, renderer and snapshot validation recognize leaves. Initial mixed canopy extraction/save/load/carve test passes under core/sync validation; expanded impact/rollback checks in progress.

**2026-10-17T21:46:49Z**

Complete: core/synchronization validation clean (6/6 body carve/leaf tests including impact, mixed composition, reservation rollback, persistence); latest full suite also passes leaf density and debris roundtrip checks. Full suite only existing ctr-ojvy REDs. Distinct leaves are loaded in running level03 scene; parallel emission adds no body-count limit. General granular per-particle appearance advection follow-up recorded on ctr-ddtf.
