---
id: ctowpe-boo0
status: closed
deps: []
links: []
created: 2026-11-27T13:41:49Z
type: task
priority: 2
assignee: kmd
parent: ctcdo-rcs9
tags: [offscreen, streaming, gas]
---
# Advance conservative gas species and pressure across resident and offscreen owner faces

## Design

Use the native gas transport and pressure policy on the connected mapped domain. Preserve each species, pigments, both momenta, energy and carrier residuals when publishing to the existing owner banks and clocks.

## Acceptance Criteria

Compare an open split face with a single world in both directions, including pressure-driven work, species, geometry and closed valves. Exercise repeated camera pans, missing neighbors and durable recovery with no duplicate transfer.


## Notes

**2026-11-27T14:07:02Z**

Native HLLC, implicit pressure and practical transport match the single-world full checkpoint, gates and clocks. Bidirectional gradients, an unmapped corner and a closed valve pass. The production camera passes three pans including a cold reopen, rejected/stale generations and retry against a single-world reference. Both targeted runs passed on hidden Xephyr :93 with Vulkan core and synchronization validation.
