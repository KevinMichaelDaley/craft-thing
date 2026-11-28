---
id: ctlo-udlu
status: in_progress
deps: []
links: []
created: 2026-10-24T10:28:55Z
type: task
priority: 1
---
# Cache full-face pressure divergence without increasing stencil storage

## Acceptance Criteria

All65537 aperture codes and arbitrary32-bit cached payloads round-trip exactly within four existing stencil words. Cache divergence only for binary hydraulic faces, retain original evaluation for partial faces, and compare all pressure/velocity/cell bytes across batches and edits. No extra mapped/device allocation, sweeps or cadence reduction; meet real displayed frame target.


## Notes

**2026-10-24T10:43:10Z**

RED minimal GPU packet probe:65537 aperture codes preserve coefficients but lose65536 payloads and32768 validity flags. GREEN exact coefficient/flag and arbitrary32-bit payload round-trip passes all65537 codes under actual core/synchronization validation, within the original four words. Uses guaranteed zero mantissa bits plus a recoverable exponent bit; pressure-sweep integration and bytewise physical oracle next.

**2026-10-24T11:03:26Z**

Integrated binary-face divergence cache within the existing four words. Actual Vulkan core+synchronization validation pressure oracle 5/5: all pressure/velocity/cell bytes identical, including 8192 mixed partial-face cells and water edits between batches. Mapped allocations identical (62218924 bytes in fixture). Full displayed benchmark next; global frame target remains unmet.

**2026-10-24T19:10:07Z**

Full coupled fluid specialization oracle exposed a divergence-cache precision mismatch even on binary faces. Removed production integration; original weight-only stencil and inline divergence preserve exact cells/velocities again. Packet helper/probe retained as an experiment. Fluid oracle1/1 and buoyancy14/14 pass after rollback.

**2026-11-28T07:18:12Z**

Production binary-face divergence caching was reinstated in f7d077ec. The runtime pressure oracle passed 9/9, including exact partial-face and edited-cell comparisons with identical 72737632-byte mapped budgets. The coupled fluid recurrence, all 65537 packed codes, one-cell transport and 17 buoyancy checks passed. The actual rendered and completed Wayland 480/640 frame pairs passed twice but the third 480x270 run reached 45.578 ms; retaining the displayed-frame gate.
