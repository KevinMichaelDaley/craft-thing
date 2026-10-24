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
