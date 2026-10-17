---
id: ctr-vjj3
status: closed
deps: []
links: []
created: 2026-10-17T17:57:00Z
type: feature
priority: 1
---
# Advect and mix water color with conserved fluid flux

## Acceptance Criteria

Imported water color follows fluid flux and mixes by incoming/resident water mass in linear RGB. Both advection and body displacement preserve pigment. No runtime CPU readback.


## Notes

**2026-10-17T18:07:13Z**

GPU RED tests demonstrated unchanged blue recipient under incoming red water. GREEN focused tests cover mass-weighted linear RGB across chunk seams, unequal mixtures/multi-cell flow, and body displacement (4/4). Separate color buffer descriptor32, total33 global bindings; imported water seeds pigment, renderer samples current water location, brush updates color and dry donors clear. Marker correction and reference paths carry pigment. Persistence/pore-moisture pigment are explicitly tracked in ctr-ddtf.

**2026-10-17T18:17:06Z**

Final validation: full headless suite completed, new pigment4/4 and existing fluid31/31 pass. Actual import verifies all1504 authored water color seeds. Core+sync validated scene smoke passes central placement/brush/12 physics ticks, no VUID/SYNC reports; rendered capture inspected. Build-dependency guard found during full run was fixed and independently rerun3/3. Only two pre-existing authored_stage_tests failures remain unresolved.
